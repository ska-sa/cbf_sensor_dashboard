#include <stdlib.h>
#include <unistd.h>
#include <katcp.h>
#include <katcl.h>
#include <netc.h>
#include <string.h>
#include <stdint.h>

#include "cmc_server.h"
#include "queue.h"
#include "utils.h"

struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port)
{
    struct cmc_server *new_cmc_server = malloc(sizeof(*new_cmc_server));
    new_cmc_server->address = strdup(address);
    new_cmc_server->katcp_port = katcp_port;
    new_cmc_server->katcp_socket_fd = net_connect(address, katcp_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
    new_cmc_server->katcl_line = create_katcl(new_cmc_server->katcp_socket_fd);
    new_cmc_server->outgoing_msg_queue = queue_create();
    queue_push(new_cmc_server->outgoing_msg_queue, "?log-local off");
    queue_push(new_cmc_server->outgoing_msg_queue, "?client-config info-all");
    queue_push(new_cmc_server->outgoing_msg_queue, "?array-list");
    new_cmc_server->current_message = NULL;
    return new_cmc_server;
}


void cmc_server_destroy(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server != NULL)
    {
        destroy_katcl(this_cmc_server->katcl_line, 1);
        close(this_cmc_server->katcp_socket_fd);
        queue_destroy(this_cmc_server->outgoing_message_queue);
        if (this_cmc_server->current_message != NULL)
        {
            free(this_cmc_server->current_message);
        }
        free(this_cmc_server->address);
        free(this_cmc_server);
    }
}


size_t cmc_server_queue_sizeof(struct cmc_server *this_cmc_server)
{
    return queue_sizeof(this_cmc_server->outgoing_msg_queue);
}


char *cmc_server_queue_pop(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->current_message != NULL)
    {
        free(this_cmc_server->current_message);
        this_cmc_server->current_message = NULL;
    }
    this_cmc_server->current_message = queue_pop(this_cmc_server->outgoing_msg_queue);
    return this_cmc_server->current_message;
}

