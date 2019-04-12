#ifndef _HTTP_HANDLING_H_
#define _HTTP_HANDLING_H_

struct webpage_buffer {
    char *buffer;
    size_t bytes_available;
    size_t bytes_written;
};

struct webpage_client {
    struct webpage_buffer *buffer;
    int fd;
    int wants_data;
};

struct webpage_buffer *create_webpage_buffer();
void destroy_webpage_buffer(struct webpage_buffer *buffer);

int add_to_buffer(struct webpage_buffer *buffer, char *html_text);
int write_buffer_to_fd(int fd, struct webpage_buffer *buffer, int bufsize);

int have_buffer_to_write(struct webpage_buffer *buffer);

struct webpage_client *create_webpage_client(int fd);
void destroy_webpage_client(struct webpage_client *client);

#endif

