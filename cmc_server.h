#ifndef _CMC_SERVER_H_
#define _CMC_SERVER_H_

struct cmc_server {
    uint16_t katcp_port;
    char *address;
    int katcp_socket_fd;
    struct katcl_line *katcl_line;
    struct queue *outgoing_msg_queue;
    char *current_message;
};

struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port);
void cmc_server_destroy(struct cmc_server *this_cmc_server);

size_t cmc_server_queue_sizeof(struct cmc_server *this_cmc_server);
char *cmc_server_queue_pop(struct cmc_server *this_cmc_server);

#endif
