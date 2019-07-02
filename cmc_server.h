#ifndef _CMC_SERVER_H_
#define _CMC_SERVER_H_

#include <sys/select.h>
#include <katcl.h>

#include "message.h"
#include "array.h"

struct cmc_server;

struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port);
void cmc_server_destroy(struct cmc_server *this_cmc_server);

void cmc_server_set_fds(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr, int *nfds);
void cmc_server_setup_katcp_writes(struct cmc_server *this_cmc_server);
void cmc_server_socket_read_write(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr);
void cmc_server_handle_received_katcl_lines(struct cmc_server *this_cmc_server);

struct message *cmc_server_queue_pop(struct cmc_server *this_cmc_server);

char *cmc_server_html_representation(struct cmc_server *this_cmc_server);

#endif
