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

static int number_of_multicast_groups(char *multicast_groups)
{
   int r = 0;
   char *temp;
   temp = strtok(multicast_groups, " ");
   if (temp)
   {
       r++;
       do {
           temp = strtok(NULL, " ");
           if (temp)
               r++;
       } while (temp);
   }
   return r;
}

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

    new_array->number_of_antennas = number_of_multicast_groups(multicast_groups) / 2;

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
    
    new_array->fhosts = malloc(sizeof(*(new_array->fhosts))*new_array->number_of_antennas);
    new_array->xhosts = malloc(sizeof(*(new_array->xhosts))*new_array->number_of_antennas);

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
    int i;
    for (i = 0; i < array->number_of_antennas; i++)
    {
        destroy_fhost(array->fhosts[i]);
        destroy_xhost(array->xhosts[i]);
    }
    free(array->fhosts);
    free(array->xhosts);
    free(array);
}

int request_functional_mapping(struct cmc_array *array)
{
    int r;
    r = append_string_katcl(array->l, KATCP_FLAG_FIRST, "?sensor-value");
    if (!(r<0))
        r = append_string_katcl(array->l, KATCP_FLAG_LAST, "hostname-functional-mapping");
    return r;
}

int accept_functional_mapping(struct cmc_array *array)
{
    int r = -1; /* -1 means the message that it got was unknown */
    if (have_katcl(array->l) > 0)
    {
        if (!strcmp(arg_string_katcl(array->l, 0), "#sensor-value") && !strcmp(arg_string_katcl(array->l, 3), "hostname-functional-mapping"))
        {
            //printf("%s\n", arg_string_katcl(array->l, 5));
            r = 1; /* one means we're getting the value we want */
        }
        else if (!strcmp(arg_string_katcl(array->l, 0), "!sensor-value"))
        {
            if (!strcmp(arg_string_katcl(array->l, 1), "ok"))
                r = 0; /* 0 means it's complete and we can move on */
        }
    }
    return r;
}

static int ss_append_string_katcl(struct katcl_line *l, char *sensor_name)
{
    int r;
    if ((r = append_string_katcl(l, KATCP_FLAG_FIRST, "?sensor-sampling")) < 0) return r;
    if ((r = append_string_katcl(l, 0, sensor_name)) < 0) return r;
    if ((r = append_string_katcl(l, KATCP_FLAG_LAST, "auto")) < 0) return r;
    return 0;
}

int request_sensor_sampling(struct cmc_array *array)
{
    printf("requesting sensors %s...\n", array->name);
    int i;
    for (i = 0; i < array->number_of_antennas; i++)
    {
        size_t needed = snprintf(NULL, 0, "fhost%02d.device-status", i) + 1;
        char *sensor_name = malloc(needed);
        sprintf(sensor_name, "fhost%02d.device-status", i);
        ss_append_string_katcl(array->l, sensor_name);
        free(sensor_name);

        needed = snprintf(NULL, 0, "xhost%02d.device-status", i) + 1;
        sensor_name = malloc(needed);
        sprintf(sensor_name, "fhost%02d.device-status", i);
        ss_append_string_katcl(array->l, sensor_name);
        free(sensor_name);
    }
    return 0;
}

int process_sensor_status(struct cmc_array *array)
{

    int r = -1; /* -1 means the message that it got was unknown */
    if (have_katcl(array->l) > 0)
    {
        if (!strcmp(arg_string_katcl(array->l, 0), "#sensor-status"))
        {
            printf("%s: %s\n", arg_string_katcl(array->l, 3), arg_string_katcl(array->l, 5));
            r = 0; 
        }
        else
            printf("%s %s %s %s\n", arg_string_katcl(array->l, 0), arg_string_katcl(array->l, 1), arg_string_katcl(array->l, 2), arg_string_katcl(array->l, 3));
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

struct fhost *create_fhost(char *hostname)
{
    struct fhost *new_fhost = malloc(sizeof(*new_fhost));
    snprintf(new_fhost->hostname, sizeof(new_fhost->hostname), "%s", hostname);

    return new_fhost;
}

void destroy_fhost(struct fhost *fhost)
{
    free(fhost);
}

struct xhost *create_xhost(char *hostname)
{
    struct xhost *new_xhost = malloc(sizeof(*new_xhost));
    snprintf(new_xhost->hostname, sizeof(new_xhost->hostname), "%s", hostname);

    return new_xhost;
}

void destroy_xhost(struct xhost *xhost)
{
    free(xhost);
}

