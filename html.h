#ifndef _HTML_HANDLING_H
#define _HTML_HANDLING_H

char *html_doctype();
char *html_open();
char *html_head_open();
char *html_title(char *title);
char *html_head_close();

char *html_body_open();
char *html_body_close();

char *html_close();

//int send_html_table_row(int socket_fd);
#endif

