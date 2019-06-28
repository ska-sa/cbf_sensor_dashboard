#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "web.h"
#include "verbose.h"

#define BUF_SIZE 1024
#define max(x,y) ((x) > (y) ? (x) : (y))

struct web_client {
    char *buffer;
    size_t bytes_available;
    size_t bytes_written;
    int fd;
};


struct web_client *web_client_create(int fd)
{
    struct web_client *new_client = malloc(sizeof(*new_client));
    new_client->buffer = malloc(1);
    new_client->buffer[0] = '\0';
    new_client->bytes_available = 0;
    new_client->bytes_written = 0;
    new_client->fd = fd;
    return new_client;
}


void web_client_destroy(struct web_client *client)
{
    int r;
    r = shutdown(client->fd, SHUT_RDWR);
    if (r < 0)
    {
        perror("shutdown");
    }
    r = close(client->fd);
    if (r < 0)
    {
        perror("close"); // for completeness, one really should be more rigorous about this...
    }

    free(client->buffer);
    free(client);
}


int web_client_buffer_add(struct web_client *client, char *html_text)
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


int web_client_buffer_write(struct web_client *client)
{
    ssize_t r;
    size_t bytes_ready = client->bytes_available - client->bytes_written;
    size_t bytes_to_write = (bytes_ready > BUF_SIZE) ? BUF_SIZE : bytes_ready;

    if (client->bytes_written == 0) /* i.e. this is a new thing, we can send the http header */
    {
        char format[] = "HTTP/1.1 200 OK\nContent-Length: %ld\nConnection: close\n\n";
        int needed = snprintf(NULL, 0, format, client->bytes_available) + 1; // snprintf can return a negative value on failure.
        if (needed < 1) //this means there was a problem somehow.
            return -1;
        char *http_ok_message = malloc((size_t) needed); // int guaranteed non-negative so can safely cast.
        sprintf(http_ok_message, format, client->bytes_available);
        r = write(client->fd, http_ok_message, strlen(http_ok_message));
        free(http_ok_message);
    }

    r = write(client->fd, client->buffer + client->bytes_written, bytes_to_write);
    if (r < 0)
        return -1; /* minus one means an error */
    client->bytes_written += (unsigned long) r; //we previously made certain it's not negative.
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


static int web_client_have_buffer(struct web_client *client)
{
    if (client->bytes_available > client->bytes_written)
        return 1;
    else
        return 0;
}


void web_client_set_fds(struct web_client *client, fd_set *rd, fd_set *wr, int *nfds)
{
    FD_SET(client->fd, rd);
    if (web_client_have_buffer(client))
    {
        FD_SET(client->fd, wr);
    }
    *nfds = max(*nfds, client->fd);
}


int web_client_socket_read(struct web_client *client, fd_set *rd)
{
    ssize_t r = 0;
    char buffer[BUF_SIZE];

    if (FD_ISSET(client->fd, rd))
    {
        r = read(client->fd, buffer, BUF_SIZE - 1);
        buffer[r] = '\0'; //just for good safety.
        verbose_message(BORING, "Received '%s' on fd %d.\n", buffer, client->fd);
    }
    return (int) r;
}

int web_client_socket_write(struct web_client *client, fd_set *wr)
{
    //int r = 0;

    if (FD_ISSET(client->fd, wr))
    {
       //TODO 
    }
    return 0;
}
