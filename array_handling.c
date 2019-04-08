#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* Needed for read() and write() */

#include "array_handling.h"

char *read_full_katcp_line(struct katcl_line *l)
{
    int i = 0;
    char* line_to_return;
    line_to_return = malloc(1);
    memset(line_to_return, '\0', 1);
    //size_t current_line_length = 0;
    char* buffer;
    do {
        buffer = arg_copy_string_katcl(l, i++);
        if (buffer)
        {
            line_to_return = realloc(line_to_return, strlen(line_to_return) + strlen(buffer) + 2);
            line_to_return = strcat(line_to_return, buffer);
            line_to_return = strcat(line_to_return, " "); /* otherwise this becomes rather unreadable. */
        }
        free(buffer);
    } while (buffer);

    //line_to_return[strlen(line_to_return) - 2] = '\0'; /* remove the trailing space */

    return line_to_return;
}

struct cmc_array *create_array(char *array_name, int monitor_port, char *multicast_groups)
{
    struct cmc_array *new_array = malloc(sizeof(*new_array));
    new_array->name = malloc(strlen(array_name) + 1);
    sprintf(new_array->name, "%s", array_name);
    new_array->monitor_port = monitor_port;
    new_array->multicast_groups = malloc(strlen(multicast_groups) + 1);
    sprintf(new_array->multicast_groups, "%s", multicast_groups);
    return new_array;
}

char *get_array_name(struct cmc_array *array)
{
    char *array_name = malloc(strlen(array->name));
    sprintf(array_name, "%s", array->name);
    return array_name;
}

void destroy_array(struct cmc_array *array)
{
    free(array->name);
    free(array->multicast_groups);
    free(array);
}

/* Function takes a port number as an argument and returns a file descriptor
 * to the resulting socket. Opens socket on 0.0.0.0. */
int listen_on_socket(int listening_port)
{
    struct sockaddr_in a;
    int s;
    int yes;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return -1;
    }

    /* This is an eccentricity of setsockopt, it needs an address and not just a value for the "1",
     * so you give it this "yes" variable.*/
    yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    {
        perror("setsockopt");
        close(s);
        return -1;
    }
    /* clear out the address struct from whatever garbage was in it. */
    memset(&a, 0, sizeof(a));
    a.sin_port = htons(listening_port);
    a.sin_family = AF_INET;
    /* TODO - explicitly make the addr point to 0.0.0.0? I guess it's not really needed. */
    if (bind(s, (struct sockaddr *) &a, sizeof(a)) == -1)
    {
        perror("bind");
        close(s);
        return -1;
    }
    printf("Accepting connections on port %d\n", listening_port);
    listen(s, 10); /* turns out 10 is a thumb-suck value but it's pretty sane. A legacy of olden times... */
    return s;
}

