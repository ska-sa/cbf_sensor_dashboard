#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "web.h"
#include "verbose.h"
#include "html.h"

#define BUF_SIZE 1024
#define max(x,y) ((x) > (y) ? (x) : (y))

struct web_client {
    char *buffer;
    size_t bytes_available;
    size_t bytes_written;
    int fd;
    int get_received;
    char *requested_resource;
};


struct web_client *web_client_create(int fd)
{
    verbose_message(BORING, "Creating web client with FD %d\n", fd);

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


void web_client_destroy(struct web_client *client)
{
    verbose_message(BORING, "Destroyong web client with FD %d\n", client->fd);

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


int web_client_buffer_add(struct web_client *client, char *html_text)
{
    /* checks because they seem to be needed */
    if (client->buffer == NULL || html_text == NULL)
        return -1;
    verbose_message(BORING, "Adding '%s' to buffer of client on FD %d.\n", html_text, client->fd);
    size_t needed = strlen(client->buffer) + strlen(html_text) + 1;
    char *temp = realloc(client->buffer, needed);
    if (temp)
    {
        client->buffer = temp;
        strcat(client->buffer, html_text);
        client->bytes_available += strlen(html_text);
        verbose_message(BORING, "New buffer: %s\n", client->buffer);
        return 0;
    }
    return -1;
}


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
            return -1;
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

    verbose_message(DEBUG, "About to write, fd=%d, buffer=0x%lx, byes_written=%d, bytes_to_write=%d\n", \
            client->fd, client->buffer, client->bytes_written, bytes_to_write);
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
            return 0; /* zero means that the send buffer is now empty */
        }
        else
            return -1;
    }
    return 1; /* one means that there's still something to send. */
}


static int web_client_have_buffer(struct web_client *client)
{
    if (client->bytes_available > client->bytes_written)
        return 1;
    else
        return 0;
}


void web_client_set_fds(struct web_client *client, fd_set *rd, fd_set *wr, int *nfds)
{
    FD_SET(client->fd, rd);
    if (web_client_have_buffer(client))
    {
        FD_SET(client->fd, wr);
    }
    *nfds = max(*nfds, client->fd);
}


int web_client_socket_read(struct web_client *client, fd_set *rd)
{
    ssize_t r = 0;
    char buffer[BUF_SIZE];

    if (FD_ISSET(client->fd, rd))
    {
        //verbose_message(BORING, "FD %d indicated it has something for us to read.\n", client->fd);
        r = read(client->fd, buffer, BUF_SIZE - 1);
        if (r<0)
        {
            perror("read");
            return -1;
        }
        if (r==0)
        {
            verbose_message(BORING, "FD %d read zero bytes, closing.\n", client->fd);
            return -1;//this just means that r has nothing left to say. No error.
        }

        buffer[r] = '\0'; //just for good safety.
        //verbose_message(BORING, "Received %ld bytes: '%s' on fd %d.\n", r, buffer, client->fd);
        //TODO take some decision on what to do with the information read.
        char first_word[BUF_SIZE];
        sprintf(first_word, "%s", strtok(buffer, " "));
        if (!strcmp(first_word, "GET"))
        {
            client->get_received = 1;
            client->requested_resource = strdup(strtok(NULL, " "));
            verbose_message(BORING, "Client on FD %d requested %s.\n", client->fd, client->requested_resource);
            return 1;
        }
        //We're basically ignoring everything except GET requests. We don't even really care about the other stuff.
    }
    return 0; //not marked for read.
}


int web_client_socket_write(struct web_client *client, fd_set *wr)
{
    int r = 0;

    if (FD_ISSET(client->fd, wr))
    {
        verbose_message(BORING, "FD %d marked for write.\n", client->fd);
        r = web_client_buffer_write(client);
    }
    return r; //no read
}


int web_client_handle_requests(struct web_client *client, struct cmc_server **cmc_list, size_t num_cmcs)
{
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
                    web_client_buffer_add(client, cmc_server_html_representation(cmc_list[i]));
                }
            }
            web_client_buffer_add(client, html_body_close());
        }
        else
        {
            char *title = html_title("CBF Sensor Dashboard, other resource requested");
            web_client_buffer_add(client, title);
            free(title);
            web_client_buffer_add(client, html_head_close());

            web_client_buffer_add(client, html_body_open());
            web_client_buffer_add(client, "Got a request for something else.\n"); //TODO This is what needs to be generated.
            web_client_buffer_add(client, html_body_close());
        }
        
        web_client_buffer_add(client, html_close());

        client->get_received = 0;
        free(client->requested_resource);
        client->requested_resource = NULL;
    }
    //otherwise ignore
    return 0;
}
