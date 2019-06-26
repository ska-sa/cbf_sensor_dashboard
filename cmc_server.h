#ifndef _CMC_SERVER_H_
#define _CMC_SERVER_H_

#include <sys/select.h>
#include <katcl.h>

#include "message.h"
#include "queue.h"
#include "array.h"

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
};

struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port);
void cmc_server_destroy(struct cmc_server *this_cmc_server);

void cmc_server_set_fds(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr, int *nfds);
void cmc_server_setup_katcp_writes(struct cmc_server *this_cmc_server);

struct message *cmc_server_queue_pop(struct cmc_server *this_cmc_server);


#endif
