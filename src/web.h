#ifndef _WEB_H_
#define _WEB_H_

#include <sys/select.h>
#include <stddef.h>

#include "cmc_server.h"
#include "cmc_aggregator.h"

/**
 * \file  web.h
 * \brief The web_client type handles HTTP connections from clients.
 */

struct web_client;

struct web_client *web_client_create(int fd);
void web_client_destroy(struct web_client *client);

int web_client_buffer_add(struct web_client *client, char *html_text);

void web_client_set_fds(struct web_client *client, fd_set *rd, fd_set *wr, int *nfds);
int web_client_socket_read(struct web_client *client, fd_set *rd);
int web_client_socket_write(struct web_client *client, fd_set *wr);

int web_client_handle_requests(struct web_client *client, struct cmc_server **cmc_list, size_t num_cmcs, struct cmc_aggregator *cmc_agg);

#endif
