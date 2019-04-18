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
    printf("FOOBAR creating...");
    struct webpage_buffer *buffer = malloc(sizeof(*buffer));
    if (buffer)
    {
        buffer->buffer = malloc(1);
        buffer->buffer[0] = 0;
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
    printf("FOOBAR destroying...");
    if (buffer->buffer != NULL)
        free(buffer->buffer);
    free(buffer);
}

int add_to_buffer(struct webpage_buffer *buffer, char *html_text)
{
    /* checks because they seem to be needed */
    if (!buffer->buffer || !html_text)
        return -1;
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

    if (buffer->bytes_written == 0) /* i.e. this is a new thing, we can send the http header */
    {
        size_t needed = snprintf(NULL, 0, "HTTP/1.1 200 OK\nContent-Length: %ld\n\n", buffer->bytes_available) + 1;
        char *http_ok_message = malloc(needed);
        sprintf(http_ok_message, "HTTP/1.1 200 OK\nContent-Length: %ld\n\n", buffer->bytes_available);
        r = write(fd, http_ok_message, strlen(http_ok_message));
        free(http_ok_message);
    }

    r = write(fd, buffer->buffer + buffer->bytes_written, bytes_to_write);
    if (r < 0)
        return -1; /* minus one means an error */
    buffer->bytes_written += r;
    if (buffer->bytes_written == buffer->bytes_available)
    {
        char *temp = realloc(buffer->buffer, 1);
        if (temp)
        {
            buffer->buffer = temp;
            buffer->buffer[0] = 0;
            buffer->bytes_available = 0;
            buffer->bytes_written = 0;
            return 0; /* zero means that the send buffer is now empty */
        }
        else
            return -1;
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
    if (client->buffer)
        destroy_webpage_buffer(client->buffer);
    free(client);
}

