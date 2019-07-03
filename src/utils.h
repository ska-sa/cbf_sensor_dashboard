#ifndef _UTILS_H_
#define _UTILS_H_

char *read_full_katcp_line(struct katcl_line *l);
int listen_on_socket(uint16_t listening_port);

#endif
