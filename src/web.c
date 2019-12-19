#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "web.h"
#include "html.h"
#include "tokenise.h"

//TODO think about moving these definitions to some central place. Could introduce bugs if not modified properly.
#define BUF_SIZE 1024
#define max(x,y) ((x) > (y) ? (x) : (y))

/// A struct to hold the information required to service an HTTP connection from a web browser.
struct web_client {
    /// The data to be sent to the client in response to an HTTP GET request.
    char *buffer;
    /// The number of bytes available in the buffer.
    size_t bytes_available;
    /// The number of bytes already written from the buffer to the file descriptor.
    size_t bytes_written;
    /// The file decsriptor associated with the connection.
    int fd;
    /// A flag indicating that the client has sent a GET and is waiting for a response.
    int get_received;
    /// The resource which the client requested.
    char *requested_resource;
};


/**
 * \fn      struct web_client *web_client_create(int fd)
 * \details Allocate memory for a web_client object and populate the members with NULL values.
 * \param   fd The file descriptor on which the browser client connetion has been made.
 * \return  A pointer to the newly-created web_client object.
 */
struct web_client *web_client_create(int fd)
{
    struct web_client *new_client = malloc(sizeof(*new_client));
    new_client->buffer = malloc(1);
    new_client->buffer[0] = '\0';
    new_client->bytes_available = 0;
    new_client->bytes_written = 0;
    new_client->fd = fd;

    new_client->get_received = 0;
    new_client->requested_resource = NULL;

    return new_client;
}


/**
 * \fn      void web_client_destroy(struct web_client *client)
 * \details Free the memory associated with the web_client object.
 * \param   client A pointer to the web_client object to be destroyed.
 * \return  void
 */
void web_client_destroy(struct web_client *client)
{
    int r;
    r = shutdown(client->fd, SHUT_RDWR);
    if (r < 0)
    {
        perror("shutdown");
    }
    r = close(client->fd);
    if (r < 0)
    {
        perror("close"); // for completeness, one really should be more rigorous about this...
    }

    free(client->buffer);
    free(client);
}


/**
 * \fn      int web_client_buffer_add(struct web_client *client, char *html_text)
 * \details Add some text to the web_client's send buffer.
 * \param   client A pointer to the web_client in question.
 * \param   html_text A string containing the text (ostensibly HTML-formatted, but not completely necessary) to be sent to the client.
 * \return  An integer indicating the success of the operation.
 */
int web_client_buffer_add(struct web_client *client, char *html_text)
{
    /* checks because they seem to be needed */
    if (client->buffer == NULL || html_text == NULL)
        return -1; /// \retval -1 The operation returned failure.
    size_t needed = strlen(client->buffer) + strlen(html_text) + 1;
    char *temp = realloc(client->buffer, needed);
    if (temp)
    {
        client->buffer = temp;
        strcat(client->buffer, html_text);
        client->bytes_available += strlen(html_text);
        return 0; /// \retval 0 The operation was successful.
    }
    return -1;
}


/**
 * \fn      static int web_client_buffer_write(struct web_client *client)
 * \details Write the buffer out to the client connection's file descriptor.
 * \param   client A pointer to the web_client in question.
 * \return  An integer indicating the success of the operation.
 */
static int web_client_buffer_write(struct web_client *client)
{
    ssize_t r;
    size_t bytes_ready = client->bytes_available - client->bytes_written;
    size_t bytes_to_write = (bytes_ready > BUF_SIZE) ? BUF_SIZE : bytes_ready;

    if (client->bytes_written == 0) /* i.e. this is a new thing, we can send the http header */
    {
        char format[] = "HTTP/1.1 200 OK\nContent-Length: %ld\nConnection: close\n\n";
        int needed = snprintf(NULL, 0, format, client->bytes_available) + 1; // snprintf can return a negative value on failure.
        if (needed < 1) //this means there was a problem somehow.
        {
            perror("snprintf");
            return -1; /// \retval -1 The operation has failed.
        }
        char *http_ok_message = malloc((size_t) needed); // int guaranteed non-negative so can safely cast.
        sprintf(http_ok_message, format, client->bytes_available);
        r = write(client->fd, http_ok_message, strlen(http_ok_message));
        if (r<0)
        {
            perror("write()");
            return -1;
        }
        if ((ssize_t) bytes_to_write + r > BUF_SIZE)
            bytes_to_write -= (size_t) r; // so we don't send more than one buffer size this round, to take
                                          // into account http ok message. cast is fine because we tested <0 previously.
        free(http_ok_message);
    }

    r = write(client->fd, client->buffer + client->bytes_written, bytes_to_write);
    if (r < 0)
    {
        perror("write()");
        return -1; /* minus one means an error */
    }
    client->bytes_written += (unsigned long) r; //we previously made certain it's not negative.
    if (client->bytes_written == client->bytes_available)
    {
        char *temp = realloc(client->buffer, 1);
        if (temp)
        {
            client->buffer = temp;
            client->buffer[0] = 0;
            client->bytes_available = 0;
            client->bytes_written = 0;
            return 0; /// \retval 0 The operation has succeeded, and the client's buffer is now empty.
        }
        else
            return -1;
    }
    return 1; /// \retval 1 The operation has succeeded, but the client's buffer still has some data left to send.
}


/**
 * \fn      static int web_client_have_buffer(struct web_client *client)
 * \details Query whether or not the buffer has data ready to be written.
 * \param   client A pointer to the client in question.
 * \return  An integer indicating whether or not the buffer has valid data.
 */
static int web_client_have_buffer(struct web_client *client)
{
    if (client->bytes_available > client->bytes_written)
        return 1; /// \retval 1 The buffer has data available.
    else
        return 0; /// \retval 0 The buffer has no data.
}


/** 
 * \fn      void web_client_set_fds(struct web_client *client, fd_set *rd, fd_set *wr, int *nfds)
 * \details Set file-descriptor sets according to what's happening in the client object. Read will always be set, but write will only
 *          ever be set if the buffer has data ready.
 * \param   client A pointer to the web_client in question.
 * \param   rd A pointer to the fd_set indicating ready to read.
 * \param   wr A pointer to the fd_set indicating ready to write.
 * \param   nfds A pointer to an integer indicating the number of file descriptors in the above sets.
 * \return  void
 */
void web_client_set_fds(struct web_client *client, fd_set *rd, fd_set *wr, int *nfds)
{
    FD_SET(client->fd, rd);
    if (web_client_have_buffer(client))
    {
        FD_SET(client->fd, wr);
    }
    *nfds = max(*nfds, client->fd);
}


/**
 * \fn      int web_client_socket_read(struct web_client *client, fd_set *rd)
 * \details Read from the web_client's file descriptor (if it's set), check what it wants. Respond only to a GET request.
 * \param   client A pointer to the web_client in question.
 * \param   rd A pointer to the fd_set indicating a read is ready.
 * \return  An integer indicating the outcome of the operation.
 */
int web_client_socket_read(struct web_client *client, fd_set *rd)
{
    ssize_t r = 0;
    char buffer[BUF_SIZE];

    if (FD_ISSET(client->fd, rd))
    {
        r = read(client->fd, buffer, BUF_SIZE - 1);
        if (r<0)
        {
            perror("read");
            return -2; /// \retval -2 The read operation failed.
        }
        if (r==0)
        {
            return -1; /// \retval -1 The file descriptor has nothing left to say. No error, but the socket probably needs closing.
        }

        buffer[r] = '\0'; //just for good safety.
        char first_word[BUF_SIZE];
        sprintf(first_word, "%s", strtok(buffer, " "));
        if (!strcmp(first_word, "GET"))
        {
            client->get_received = 1;
            client->requested_resource = strdup(strtok(NULL, " "));
            //syslog(LOG_DEBUG, "Client on FD %d requested %s.", client->fd, client->requested_resource);
            return 1; /// \retval 1 Read successful, GET request identified.
        }
        //We're basically ignoring everything except GET requests. We don't even really care about the other stuff.
    }
    return 0; /// \retval 0 This client's file descriptor is not marked for read (should not occur in normal operation) or 
              ///           the received data was not a GET request.
    ///TODO At the moment this is just quick-and-dirty - check for a GET and what resource was requested. Decide whether or not 
    ///to make it fully compliant with the HTML standards.
}


/**
 * \fn      int web_client_socket_write(struct web_client *client, fd_set *wr)
 * \details Write to the web_client's file descriptor, if it is available.
 * \param   client A pointer to the web_client in question.
 * \param   wr A pointter to the df_set indicating that a write is ready.
 * \return  An integer indicating the outcome of the operation.
 */
int web_client_socket_write(struct web_client *client, fd_set *wr)
{
    int r = 0;

    if (FD_ISSET(client->fd, wr))
    {
        r = web_client_buffer_write(client);
    }
    return r; //no read
}


/**
 * \fn      int web_client_handle_requests(struct web_client *client, struct cmc_server **cmc_list, size_t num_cmcs, struct cmc_aggregator *cmc_agg)
 * \details Compose a response to the client based on the requested resource, and the current state of stored data. Push the composed response onto the
 *          web_client's buffer for sending when it's ready.
 * \param   client A pointer to the web_client in question.
 * \param   cmc_list A pointer to the program's list of cmc_server objects, to be able to retrieve the data needed to compose a response.
 * \param   num_cmcs The number of cmc_server objects in the list.
 * \param   cmc_agg The aggregator of all the arrays in all the cmc_server objects, so that we can access the array objects directly if need be.
 * \return  At present, this function always returns zero to indicate success. Chances of failure are pretty low on modern systems...
 */
int web_client_handle_requests(struct web_client *client, struct cmc_server **cmc_list, size_t num_cmcs, struct cmc_aggregator *cmc_agg)
{
    //TODO: check requested resource before sending anything. Probably the correct thing to do is to
    //send a 404 in that case.
    if (client->get_received == 1)
    {
        web_client_buffer_add(client, html_doctype());
        web_client_buffer_add(client, html_open());
        web_client_buffer_add(client, html_head_open());

        if (!strcmp(client->requested_resource, "/"))
        {
            char *title = html_title("CBF Sensor Dashboard");
            web_client_buffer_add(client, title);
            free(title);
            web_client_buffer_add(client, html_script());

            char *styles = html_style();
            web_client_buffer_add(client, styles);
            free(styles);

            web_client_buffer_add(client, html_head_close());

            web_client_buffer_add(client, html_body_open());
            if (!num_cmcs)
            {
                web_client_buffer_add(client, "<p>It appears that no CMCs are online at this time.</p>\n");
            }
            else
            {
                size_t i;
                for (i = 0; i < num_cmcs; i++)
                {
                    char *cmc_server_html_rep = cmc_server_html_representation(cmc_list[i]);
                    web_client_buffer_add(client, cmc_server_html_rep);
                    free(cmc_server_html_rep);
                }
            }
        }
        else
        {
            char **tokens = NULL;
            size_t n_tokens = tokenise_string(client->requested_resource, '/', &tokens);

            char *requested_cmc;
            char *requested_array = strdup("");
            int requested_missing_pkts = 0;

            switch (n_tokens) {
                default:
                syslog(LOG_WARNING, "Requested URL (%s) too long. Expect <cmc>/<array_name> only. Ignoring everything else.", client->requested_resource);
                case 3: // we're ignoring the actual content of the third token, but if it's there, we'll show missing-pkts.
                    requested_missing_pkts = 1;
                case 2: // This means, we're requesting an array that's in one of the CMCs.
                    free(requested_array);
                    requested_array = strdup(tokens[1]);
                case 1: // This means we're just requesting an array directly.
                    requested_cmc = strdup(tokens[0]);
            }

            {
                char format[] = "CBF Sensor Dashboard: %s/%s";
                ssize_t needed = snprintf(NULL, 0, format, requested_cmc, requested_array) + 1;
                char *title_string = malloc((size_t) needed);
                sprintf(title_string, format, requested_cmc, requested_array);
                char *title = html_title(title_string);
                web_client_buffer_add(client, title);
                free(title_string);
                free(title);

                char *styles = html_style();
                web_client_buffer_add(client, styles);
                free(styles);

                web_client_buffer_add(client, html_script());
                web_client_buffer_add(client, html_head_close());
            }

            web_client_buffer_add(client, html_body_open());

            if (strcmp("", requested_array)) //will return a true value if they are not equal, i.e. an array has been requested.
            {
                size_t i;
                for (i = 0; i < num_cmcs; i++)
                {
                    if (!strcmp(requested_cmc, cmc_server_get_name(cmc_list[i])))
                        break;
                }
                if (i == num_cmcs)
                {
                    char format[] = "<p>No cmc named %s.</p>";
                    ssize_t needed = snprintf(NULL, 0, format, requested_cmc) + 1;
                    char *message = malloc((size_t) needed);
                    sprintf(message, format, requested_cmc);
                    web_client_buffer_add(client, message);
                    free(message);
                }
                else
                {
                    int r = cmc_server_check_for_array(cmc_list[i], requested_array);
                    if (r >= 0)
                    {
                        if (requested_missing_pkts)
                        {
                            char *missing_pkts_detail = array_html_missing_pkt_view(cmc_server_get_array(cmc_list[i], (size_t) r));
                            web_client_buffer_add(client, missing_pkts_detail);
                            free(missing_pkts_detail);
                        }
                        else
                        {
                            char *array_detail = array_html_detail(cmc_server_get_array(cmc_list[i], (size_t) r));
                            web_client_buffer_add(client, array_detail);
                            free(array_detail);
                        }
                    }
                    else
                    {
                        char format[] = "<p>%s does not have an array named %s.</p>";
                        ssize_t needed = snprintf(NULL, 0, format, requested_cmc, requested_array) + 1;
                        char *message = malloc((size_t) needed);
                        sprintf(message, format, requested_cmc, requested_array);
                        web_client_buffer_add(client, message);
                        free(message);
                    }
                }
            }
            else // i.e. only one "token" in the requested resource, client has asked for an array directly.
            {

                int i;
                struct array *nth_array = NULL;

                //check that the token given is a number.
                for (i = 0; i < strlen(requested_cmc); i++)
                {
                    if (!isdigit(requested_cmc[i]))
                        break;
                }
                if (i == strlen(requested_cmc)) //i.e. no breaks out of loop, all elements are digits.
                {
                    size_t r = (size_t) atoi(requested_cmc);
                    nth_array = cmc_aggregator_get_array(cmc_agg, r - 1); // minus one so that we can start indexing at 1.
                }
                if (nth_array != NULL)
                {
                    char *array_detail = array_html_detail(nth_array);
                    web_client_buffer_add(client, array_detail);
                    free(array_detail);
                }
                else
                {
                    char format[] = "<p>Requsted array %s not accessible! Are you sure it's there?";
                    ssize_t needed = snprintf(NULL, 0, format, requested_cmc) + 1;
                    char *message = malloc((size_t) needed);
                    sprintf(message, format, requested_cmc);
                    web_client_buffer_add(client, message);
                    free(message);
                }
            }

            int i;
            for (i = 0; i < n_tokens; i++)
                free(tokens[i]);
            free(tokens);
            free(requested_cmc);
            free(requested_array);
        }

        web_client_buffer_add(client, html_body_close());
        web_client_buffer_add(client, html_close());

        client->get_received = 0;
        free(client->requested_resource);
        client->requested_resource = NULL;

    }
    //otherwise ignore
    return 0;
}
