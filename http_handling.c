#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "http_handling.h"

struct webpage_buffer *create_webpage_buffer()
{
    struct webpage_buffer *buffer = malloc(sizeof(*buffer));
    if (buffer)
    {
        buffer->buffer = malloc(1);
        buffer->buffer[0] = '\0';
        buffer->bytes_available = 0;
        buffer->bytes_written = 0;
        return buffer;
    }
    else
    {
        return NULL;
    }
}

void destroy_webpage_buffer(struct webpage_buffer *buffer)
{
    free(buffer->buffer);
    free(buffer);
}

int add_to_buffer(struct webpage_buffer *buffer, char *html_text)
{
    size_t needed = strlen(buffer->buffer) + strlen(html_text) + 1;
    char *temp = realloc(buffer->buffer, needed);
    if (temp)
    {
        buffer->buffer = temp;
        strcat(buffer->buffer, html_text);
        buffer->bytes_available += strlen(html_text);
        return 0;
    }
    return -1;
}

int write_buffer_to_fd(int fd, struct webpage_buffer *buffer, int bufsize)
{
    int r;
    int bytes_ready = buffer->bytes_available - buffer->bytes_written;
    int bytes_to_write = (bytes_ready > bufsize) ? bufsize : bytes_ready;
    r = write(fd, buffer->buffer + buffer->bytes_written, bytes_to_write);
    if (r < 0)
        return -1; /* minus one means an error */
    buffer->bytes_written += r;
    if (buffer->bytes_written == buffer->bytes_available)
    {
        destroy_webpage_buffer(buffer);
        buffer = create_webpage_buffer();
        return 0; /* zero means that the send buffer is now empty */
    }
    return 1; /* one means that there's still something to send. */
}


int have_buffer_to_write(struct webpage_buffer *buffer)
{
    if (buffer->bytes_available > buffer->bytes_written)
        return 1;
    else
        return 0;
}


struct webpage_client *create_webpage_client(int fd)
{
    struct webpage_client *new_client = malloc(sizeof(*new_client));
    new_client->buffer = create_webpage_buffer();
    new_client->fd = fd;
    new_client->wants_data = 0;
    return new_client;
}

void destroy_webpage_client(struct webpage_client *client)
{
    destroy_webpage_buffer(client->buffer);
    free(client);
}

