#ifndef _HTML_HANDLING_H
#define _HTML_HANDLING_H

struct webpage_buffer;

int send_html_header(struct webpage_buffer *buffer);
int send_html_body_open(struct webpage_buffer *buffer);
int send_html_body_close(struct webpage_buffer *buffer);

int send_html_section_start(struct webpage_buffer *buffer);
int send_html_section_end(struct webpage_buffer *buffer);

int send_html_paragraph(struct webpage_buffer *buffer, char *line);

int send_html_table_start(struct webpage_buffer *buffer);
int send_html_table_end(struct webpage_buffer *buffer);
int send_html_table_arraylist_header(struct webpage_buffer *buffer);
int send_html_table_arraylist_row(struct webpage_buffer *buffer, struct cmc_array *array);

int send_html_table_sensor_row(struct webpage_buffer *buffer, struct fhost *fhost, struct xhost *xhost);
//int send_html_table_row(int socket_fd);
#endif

