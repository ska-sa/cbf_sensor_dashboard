#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* Needed for read() and write() */
#include <katcp.h>
#include <katcl.h>
#include <netc.h>
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

struct cmc_array *create_array(char *array_name, int monitor_port, char *multicast_groups, char* cmc_address)
{
    struct cmc_array *new_array = malloc(sizeof(*new_array));

    new_array->name = malloc(strlen(array_name) + 1);
    sprintf(new_array->name, "%s", array_name);

    new_array->multicast_groups = malloc(strlen(multicast_groups) + 1);
    sprintf(new_array->multicast_groups, "%s", multicast_groups);

    new_array->monitor_port = monitor_port;
    new_array->monitor_socket_fd = net_connect(cmc_address, new_array->monitor_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
    if (new_array->monitor_socket_fd == -1)
    {
        fprintf(stderr, "unable to connect to array %s monitor port %d...\n", array_name, monitor_port);
        free(new_array->multicast_groups);
        free(new_array->name);
        free(new_array);
        return NULL;
    }
    new_array->l = create_katcl(new_array->monitor_socket_fd);
    new_array->state = REQUEST_FUNCTIONAL_MAPPING;
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
    destroy_katcl(array->l, 1);
    shutdown(array->monitor_socket_fd, SHUT_RDWR);
    close(array->monitor_socket_fd);
    free(array);
}

int request_functional_mapping(struct cmc_array *array)
{
    int r;
    append_string_katcl(array->l, KATCP_FLAG_FIRST, "?sensor-value");
    append_string_katcl(array->l, KATCP_FLAG_LAST, "hostname-functional-mapping");
    printf("Requesting functional mapping from array %s on port %d\n", array->name, array->monitor_port);
    r = write_katcl(array->l);
    return r;
}

int accept_functional_mapping(struct cmc_array *array)
{
    int r;
    r = read_katcl(array->l);
    if (r)
    {
        fprintf(stderr, "read failed: %s\n", (r < 0) ? strerror(error_katcl(array->l)) : "connection terminated");
        perror("read_katcl");
    }

    r = 2; /* In case the have_katcl returns nothing, we don't want the function to succeed by accident. */

    while (have_katcl(array->l) > 0)
    {
        if (!strcmp(arg_string_katcl(array->l, 0), "#sensor-value"))// && !strcmp(arg_string_katcl(array->l, 3), "hostname-functional-mapping"))
        {
            int i = 0;
            do {
                printf("%d: %s\n", i, arg_string_katcl(array->l, i));
            } while (arg_string_katcl(array->l, i++) != NULL);
            printf("\n");
            r = 1;
        }
        else if (!strcmp(arg_string_katcl(array->l, 0), "!sensor-value"))
        {
            if (!strcmp(arg_string_katcl(array->l, 1), "ok"))
                r = 0;
        }
        else
        {
            printf("katcp message seems to be unknown: \n");
            int i = 0;
            do {
                printf("%d: %s\n", i, arg_string_katcl(array->l, i));
            } while (arg_string_katcl(array->l, i++) != NULL);
            printf("\n");
        }
    }
    
    return r;
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

