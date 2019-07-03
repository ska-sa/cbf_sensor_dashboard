#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <katcp.h>
#include <katcl.h>
#include <netc.h>
#include <string.h>
#include <stdint.h>

#include "verbose.h"

#include "cmc_server.h"
#include "queue.h"
#include "message.h"
#include "utils.h"
#include "array.h"

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

enum cmc_state {
    CMC_SEND_FRONT_OF_QUEUE,
    CMC_WAIT_RESPONSE,
    CMC_MONITOR,
};


struct cmc_server {
    uint16_t katcp_port;
    char *address;
    int katcp_socket_fd;
    struct katcl_line *katcl_line;
    enum cmc_state state;
    struct queue *outgoing_msg_queue;
    struct message *current_message;
    struct array **array_list;
    size_t no_of_arrays;
};


struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port)
{
    struct cmc_server *new_cmc_server = malloc(sizeof(*new_cmc_server));
    new_cmc_server->address = strdup(address);
    new_cmc_server->katcp_port = katcp_port;
    new_cmc_server->katcp_socket_fd = net_connect(address, katcp_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
    new_cmc_server->katcl_line = create_katcl(new_cmc_server->katcp_socket_fd);
    new_cmc_server->outgoing_msg_queue = queue_create();

    new_cmc_server->array_list = NULL;
    new_cmc_server->no_of_arrays = 0;

    /*This bit is hardcoded for the time being. Perhaps a better way would be to include it in a config
     * file like the sensors to which we'll be subscribing. */
    struct message *new_message = message_create('?');
    message_add_word(new_message, "log-local");
    message_add_word(new_message, "off");
    queue_push(new_cmc_server->outgoing_msg_queue, new_message);

    new_message = message_create('?');
    message_add_word(new_message, "client-config");
    message_add_word(new_message, "info-all");
    queue_push(new_cmc_server->outgoing_msg_queue, new_message);

    new_message = message_create('?');
    message_add_word(new_message, "array-list");
    queue_push(new_cmc_server->outgoing_msg_queue, new_message);

    new_cmc_server->current_message = NULL;
    cmc_server_queue_pop(new_cmc_server);
    new_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
    return new_cmc_server;
}


void cmc_server_destroy(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server != NULL)
    {
        destroy_katcl(this_cmc_server->katcl_line, 1);
        close(this_cmc_server->katcp_socket_fd);
        queue_destroy(this_cmc_server->outgoing_msg_queue);
        message_destroy(this_cmc_server->current_message);
        free(this_cmc_server->address);
        free(this_cmc_server);
    }
}


struct message *cmc_server_queue_pop(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->current_message != NULL)
    {
        message_destroy(this_cmc_server->current_message);
    }
    this_cmc_server->current_message = queue_pop(this_cmc_server->outgoing_msg_queue);
    return this_cmc_server->current_message;
}


void cmc_server_set_fds(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr, int *nfds)
{
    FD_SET(this_cmc_server->katcp_socket_fd, rd);
    if (flushing_katcl(this_cmc_server->katcl_line))
    {
        FD_SET(this_cmc_server->katcp_socket_fd, wr);
    }
    *nfds = max(*nfds, this_cmc_server->katcp_socket_fd);
}


void cmc_server_setup_katcp_writes(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->current_message)
    {
        if (this_cmc_server->state == CMC_SEND_FRONT_OF_QUEUE)
        {
            int n = message_get_number_of_words(this_cmc_server->current_message);
            if (n > 0)
            {
                char *composed_message = message_compose(this_cmc_server->current_message);
                verbose_message(DEBUG, "Sending KATCP message \"%s\" to %s:%hu\n", composed_message, this_cmc_server->address, this_cmc_server->katcp_port);
                free(composed_message);
                composed_message = NULL;

                char *first_word = malloc(strlen(message_see_word(this_cmc_server->current_message, 0)) + 2);
                sprintf(first_word, "%c%s", message_get_type(this_cmc_server->current_message), message_see_word(this_cmc_server->current_message, 0));
                if (message_get_number_of_words(this_cmc_server->current_message) == 1)
                    append_string_katcl(this_cmc_server->katcl_line, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, first_word);
                else
                {
                    append_string_katcl(this_cmc_server->katcl_line, KATCP_FLAG_FIRST, first_word);
                    size_t j;
                    for (j = 1; j < n - 1; j++)
                    {
                        append_string_katcl(this_cmc_server->katcl_line, 0, message_see_word(this_cmc_server->current_message, j));
                    }
                    append_string_katcl(this_cmc_server->katcl_line, KATCP_FLAG_LAST, message_see_word(this_cmc_server->current_message, (size_t) n - 1));
                }
                free(first_word);
                first_word = NULL;
            }
            else
            {
                verbose_message(WARNING, "Message on %s:%hu's queue had 0 words in it.\n", this_cmc_server->address, this_cmc_server->katcp_port);
            }
            this_cmc_server->state = CMC_WAIT_RESPONSE;
        }
    }
}


void cmc_server_socket_read_write(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr)
{
    int r;
    if (FD_ISSET(this_cmc_server->katcp_socket_fd, rd))
    {
        verbose_message(BORING, "Reading katcl_line from %s:%hu.\n", this_cmc_server->address, this_cmc_server->katcp_port);
        r = read_katcl(this_cmc_server->katcl_line);
        if (r)
        {
            fprintf(stderr, "read from %s:%hu failed\n", this_cmc_server->address, this_cmc_server->katcp_port);
            perror("read_katcl()");
            /*TODO some kind of error checking, what to do if the CMC doesn't connect.*/
        }
    }

    if (FD_ISSET(this_cmc_server->katcp_socket_fd, wr))
    {
        verbose_message(BORING, "Writing katcl_line to %s:%hu.\n", this_cmc_server->address, this_cmc_server->katcp_port);
        r = write_katcl(this_cmc_server->katcl_line);
        if (r < 0)
        {
            perror("write_katcl");
            /*TODO some other kind of error checking.*/
        }
    }
}


static int cmc_server_add_array(struct cmc_server *this_cmc_server, char *array_name, uint16_t monitor_port, size_t number_of_antennas)
{
    /* TODO Check if the array name already exists.*/
    size_t i;
    for (i = 0; i < this_cmc_server->no_of_arrays; i++)
    {
        if (!strcmp(array_name, array_get_name(this_cmc_server->array_list[i])))
        {
            verbose_message(WARNING, "Attempting to add array \"%s\" to %s:%hu while an array of this name already exists.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
            return (int) i;
        }
    }

    /* If not, allocate space for it on the end. */
    struct array **temp = realloc(this_cmc_server->array_list, sizeof(*(this_cmc_server->array_list))*(this_cmc_server->no_of_arrays + 1));
    if (temp == NULL)
    {
        verbose_message(ERROR, "Unable to realloc memory to add array \"%s\" to %s:%hu.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
        return -1;
    }
    this_cmc_server->array_list = temp;
    this_cmc_server->array_list[this_cmc_server->no_of_arrays] = array_create(array_name, this_cmc_server->address, monitor_port, number_of_antennas);
    if (this_cmc_server->array_list[this_cmc_server->no_of_arrays] == NULL)
    {
        verbose_message(ERROR, "Unable to create array \"%s\" on %s:%hu.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
        return -1;
    }
    verbose_message(INFO, "Added array \"%s\" to %s:%hu.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
    this_cmc_server->no_of_arrays++;
    return 0;
}


void cmc_server_handle_received_katcl_lines(struct cmc_server *this_cmc_server)
{
    while (have_katcl(this_cmc_server->katcl_line) > 0)
    {
        verbose_message(BORING, "From %s:%hu: %s %s %s %s %s\n", this_cmc_server->address, this_cmc_server->katcp_port, \
                arg_string_katcl(this_cmc_server->katcl_line, 0), \
                arg_string_katcl(this_cmc_server->katcl_line, 1), \
                arg_string_katcl(this_cmc_server->katcl_line, 2), \
                arg_string_katcl(this_cmc_server->katcl_line, 3), \
                arg_string_katcl(this_cmc_server->katcl_line, 4)); 
        char received_message_type = arg_string_katcl(this_cmc_server->katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, message_see_word(this_cmc_server->current_message, 0)))
                {
                    if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 1), "ok"))
                    {
                        verbose_message(INFO, "%s:%hu received %s ok!\n", this_cmc_server->address, this_cmc_server->katcp_port, message_see_word(this_cmc_server->current_message, 0));
                        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                        verbose_message(DEBUG, "%s:%hu still has %u message(s) in its queue...\n", this_cmc_server->address, this_cmc_server->katcp_port, queue_sizeof(this_cmc_server->outgoing_msg_queue));
                        if (queue_sizeof(this_cmc_server->outgoing_msg_queue))
                        {
                            verbose_message(BORING, "%s:%hu  popping queue...\n", this_cmc_server->address, this_cmc_server->katcp_port);
                            cmc_server_queue_pop(this_cmc_server);
                        }
                        else
                        {
                            verbose_message(INFO, "%s:%hu going into monitoring state.\n", this_cmc_server->address, this_cmc_server->katcp_port);
                            message_destroy(this_cmc_server->current_message);
                            this_cmc_server->current_message = NULL; //doesn't do this in the above function. C problem.
                            this_cmc_server->state = CMC_MONITOR;
                        }
                    }
                    else 
                    {
                        verbose_message(WARNING, "Received %s %s. Retrying the request...", message_see_word(this_cmc_server->current_message, 0), arg_string_katcl(this_cmc_server->katcl_line, 1));
                        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                    }

                }
                break;
            case '#': // it's a katcp inform
                /*TODO handle the array-list stuff. code should be easy enough to copy from previous attempt.*/
                if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, "array-list"))
                {
                    char* array_name = arg_string_katcl(this_cmc_server->katcl_line, 1);
                    strtok(arg_string_katcl(this_cmc_server->katcl_line, 2), ","); /* don't need the first bit, that's the control port */
                    uint16_t monitor_port = (uint16_t) atoi(strtok(NULL, ","));
                    /* will leave this here while I can't think of anything to do with the multicast groups.
                    int j = 3;
                    char *multicast_groups = malloc(1);
                    multicast_groups[0] = '\0';
                    char *buffer;
                    do {
                        buffer = arg_string_katcl(this_cmc_server->katcl_line, j);
                        if (buffer)
                        {
                            multicast_groups = realloc(multicast_groups, strlen(multicast_groups) + strlen(buffer) + 2);
                            strcat(multicast_groups, " ");
                            strcat(multicast_groups, buffer);
                        }
                        j++;
                    } while (buffer);
                    free(multicast_groups);
                    multicast_groups = NULL; */

                    //count the number of multicast groups, number of antennas is this /2.
                    size_t j = 3;
                    char * buffer;
                    do {
                        buffer = arg_string_katcl(this_cmc_server->katcl_line, (uint32_t) j);
                        j++;
                    } while (buffer);
                    size_t number_of_antennas = j - 4; //to take into account the ++ which will have followed the null buffer

                    cmc_server_add_array(this_cmc_server, array_name, monitor_port, number_of_antennas);
                }
                break;
            default:
                verbose_message(WARNING, "Unexpected KATCP message received, starting with %c\n", received_message_type);
        }
    }
}


/* CMC server will be represented as an H1 with a table of the arrays beneath it.
 * for the time being, just its name.
 */
char *cmc_server_html_representation(struct cmc_server *this_cmc_server)
{
    char *cmc_html_rep;
    if (this_cmc_server->no_of_arrays < 1)
    {
        char format[] = "<h1>%s</h1>\n<p>No arrays currently running.</p>\n";
        ssize_t needed = snprintf(NULL, 0, format, this_cmc_server->address) + 1;
        cmc_html_rep = malloc((size_t) needed);
        sprintf(cmc_html_rep, format, this_cmc_server->address);
        return cmc_html_rep;
    }

    {   //putting this in its own block so that I can reuse the names "format" and "needed" later.
        //might not be ready since this is followed by a for-loop, but anyway.
        char format[] = "<h1>%s</h1>\n<table>\n";
        ssize_t needed = snprintf(NULL, 0, format, this_cmc_server->address) + 1;
        //TODO checks
        cmc_html_rep = malloc((size_t) needed);
        sprintf(cmc_html_rep, format, this_cmc_server->address);
    }
    
    size_t i;
    for (i = 0; i < this_cmc_server->no_of_arrays; i++)
    {
        char format[] = "%s%s\n";
        char *array_html_rep = array_html_summary(this_cmc_server->array_list[i]);
        ssize_t needed = snprintf(NULL, 0, format, cmc_html_rep, array_html_rep) + 1;
        //TODO checks
        cmc_html_rep = realloc(cmc_html_rep, (size_t) needed); //naughty naughty, no temp variable.
        sprintf(cmc_html_rep, format, cmc_html_rep, array_html_rep);
        free(array_html_rep);
    }

    {
        char *format = "%s</table>\n";
        ssize_t needed = snprintf(NULL, 0, format, cmc_html_rep) + 1;
        //TODO checks
        cmc_html_rep = realloc(cmc_html_rep, (size_t) needed);
        sprintf(cmc_html_rep, format, cmc_html_rep);
    }

    return cmc_html_rep;
}
