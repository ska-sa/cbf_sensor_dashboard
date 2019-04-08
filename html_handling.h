#ifndef _HTML_HANDLING_H
#define _HTML_HANDLING_H

int send_http_ok(int socket_fd);

int send_html_header(int socket_fd);
int send_html_body_open(int socket_fd);
int send_html_body_close(int socket_fd);

int send_html_section_start(int socket_fd);
int send_html_section_end(int socket_fd);

int send_html_table_start(int socket_fd);
int send_html_table_end(int socket_fd);
int send_html_table_arraylist_header(int socket_fd);
int send_html_table_arraylist_row(int socket_fd, struct cmc_array *array);

//int send_html_table_row(int socket_fd);
#endif

