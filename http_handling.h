#ifndef _HTTP_HANDLING_H_
#define _HTTP_HANDLING_H_

struct webpage_client {
    char *buffer;
    size_t bytes_available;
    size_t bytes_written;
    int fd;
    int wants_data;
};

int add_to_buffer(struct webpage_client *client, char *html_text);
int write_buffer_to_fd(struct webpage_client *client, int bufsize);

int have_buffer_to_write(struct webpage_client *client);

struct webpage_client *create_webpage_client(int fd);
void destroy_webpage_client(struct webpage_client *client);
void print_webpage_client(struct webpage_client *client);
#endif

