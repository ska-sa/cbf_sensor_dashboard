#ifndef _WEB_H_
#define _WEB_H_

#include <sys/select.h>

struct web_client;

struct web_client *web_client_create(int fd);
void web_client_destroy(struct web_client *client);

int web_client_buffer_add(struct web_client *client, char *html_text);
//int web_client_buffer_write(struct web_client *client);
//int web_client_have_buffer(struct web_client *client);

void web_client_set_fds(struct web_client *client, fd_set *rd, fd_set *wr, int *nfds);
int web_client_socket_read(struct web_client *client, fd_set *rd);
int web_client_socket_write(struct web_client *client, fd_set *wr);
#endif

