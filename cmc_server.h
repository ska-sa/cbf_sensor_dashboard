#ifndef _CMC_SERVER_H_
#define _CMC_SERVER_H_

struct cmc_server {
    uint16_t katcp_port;
    char *address;
    int katcp_socket_fd;
    struct katcl_line *katcl_line;
};

struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port);
void cmc_server_destroy(struct cmc_server *this_cmc_server);

#endif
