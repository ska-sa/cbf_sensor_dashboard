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

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port)
{
    struct cmc_server *new_cmc_server = malloc(sizeof(*new_cmc_server));
    new_cmc_server->address = strdup(address);
    new_cmc_server->katcp_port = katcp_port;
    new_cmc_server->katcp_socket_fd = net_connect(address, katcp_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
    new_cmc_server->katcl_line = create_katcl(new_cmc_server->katcp_socket_fd);
    new_cmc_server->outgoing_msg_queue = queue_create();

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
