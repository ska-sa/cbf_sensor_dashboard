#ifndef _HTML_HANDLING_H
#define _HTML_HANDLING_H

struct webpage_client;

int html_send_header(struct webpage_client *client);
int send_html_body_open(struct webpage_client *client);
int send_html_body_close(struct webpage_client *client);

int send_html_section_start(struct webpage_client *client);
int send_html_section_end(struct webpage_client *client);

int send_html_paragraph(struct webpage_client *client, char *line);

int send_html_table_start(struct webpage_client *client);
int send_html_table_end(struct webpage_client *client);
int send_html_table_arraylist_header(struct webpage_client *client);
int send_html_table_arraylist_row(struct webpage_client *client, struct cmc_array *array);

int send_html_table_sensor_row(struct webpage_client *client, struct fhost *fhost, struct xhost *xhost);

int send_quad(struct webpage_client *client);

//int send_html_table_row(int socket_fd);
#endif

