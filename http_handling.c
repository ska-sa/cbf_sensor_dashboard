#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "http_handling.h"

int add_to_buffer(struct webpage_client *client, char *html_text)
{
    /* checks because they seem to be needed */
    if (client->buffer == NULL || html_text == NULL)
        return -1;
    size_t needed = strlen(client->buffer) + strlen(html_text) + 1;
    char *temp = realloc(client->buffer, needed);
    if (temp)
    {
        client->buffer = temp;
        strcat(client->buffer, html_text);
        client->bytes_available += strlen(html_text);
        return 0;
    }
    return -1;
}

int write_buffer_to_fd(struct webpage_client *client, int bufsize)
{
    int r;
    int bytes_ready = client->bytes_available - client->bytes_written;
    int bytes_to_write = (bytes_ready > bufsize) ? bufsize : bytes_ready;

    if (client->bytes_written == 0) /* i.e. this is a new thing, we can send the http header */
    {
        char format[] = "HTTP/1.1 200 OK\nContent-Length: %ld\nConnection: close\n\n";
        size_t needed = snprintf(NULL, 0, format, client->bytes_available) + 1;
        char *http_ok_message = malloc(needed);
        sprintf(http_ok_message, format, client->bytes_available);
        r = write(client->fd, http_ok_message, strlen(http_ok_message));
        free(http_ok_message);
    }

    r = write(client->fd, client->buffer + client->bytes_written, bytes_to_write);
    if (r < 0)
        return -1; /* minus one means an error */
    client->bytes_written += r;
    if (client->bytes_written == client->bytes_available)
    {
        char *temp = realloc(client->buffer, 1);
        if (temp)
        {
            client->buffer = temp;
            client->buffer[0] = 0;
            client->bytes_available = 0;
            client->bytes_written = 0;
            return 0; /* zero means that the send buffer is now empty */
        }
        else
            return -1;
    }
    return 1; /* one means that there's still something to send. */
}

int have_buffer_to_write(struct webpage_client *client)
{
    if (client->bytes_available > client->bytes_written)
        return 1;
    else
        return 0;
}

struct webpage_client *create_webpage_client(int fd)
{
    struct webpage_client *new_client = malloc(sizeof(*new_client));
    new_client->buffer = malloc(1);
    new_client->buffer[0] = '\0';
    new_client->bytes_available = 0;
    new_client->bytes_written = 0;
    new_client->fd = fd;
    new_client->wants_data = 0;
    return new_client;
}

void destroy_webpage_client(struct webpage_client *client)
{
    free(client->buffer);
    free(client);
}

