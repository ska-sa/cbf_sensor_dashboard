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
    
    new_array->host_counter = 0;
    new_array->fhosts = malloc(sizeof(*(new_array->fhosts))*new_array->number_of_antennas);
    new_array->xhosts = malloc(sizeof(*(new_array->xhosts))*new_array->number_of_antennas);
        /* we're not actually going to create the fhosts yet, that is done by the functional mapping */
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
    if (array->current_sensor_name)
        free(array->current_sensor_name);
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
    printf("Requesting functional mapping on %s\n", array->name);
    int r;
    r = append_string_katcl(array->l, KATCP_FLAG_FIRST, "?sensor-value");
    if (!(r<0))
        r = append_string_katcl(array->l, KATCP_FLAG_LAST, "hostname-functional-mapping");
    return r;
}

int accept_functional_mapping(struct cmc_array *array)
{
    int r = -1; /* -1 means the message that it got was unknown */
    {
        if (!strcmp(arg_string_katcl(array->l, 0), "#sensor-value") && !strcmp(arg_string_katcl(array->l, 3), "hostname-functional-mapping"))
        {
            //printf("%s\n", arg_string_katcl(array->l, 5));
            int i;
            for (i = 0; i < 2*array->number_of_antennas; i++)
            {
                char host_type = arg_string_katcl(array->l, 5)[i*30 + 21];
                int host_number = atoi(strndup(arg_string_katcl(array->l, 5) + (i*30 + 26), 2));
                char *hostname = strndup(arg_string_katcl(array->l, 5) + (i*30 + 8), 6);
                switch (host_type)
                {
                    case 'f':
                        printf("Found %s-fhost%02d on physical host skarab%s-01\n", array->name, host_number, hostname);
                        array->fhosts[host_number] = create_fhost(hostname, host_number);
                        break;
                    case 'x':
                        printf("Found %s-xhost%02d on physical host skarab%s-01\n", array->name, host_number, hostname);
                        array->xhosts[host_number] = create_xhost(hostname, host_number);
                        break;
                    default:
                        printf("Couldn't parse array properly: got %c from position %d of [%s]\n", host_type, (i*30 + 21), arg_string_katcl(array->l, 5));
                }
                free(hostname);
            }
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
    printf("subscribing to sensor %s\n", sensor_name);
    int r;
    if ((r = append_string_katcl(l, KATCP_FLAG_FIRST, "?sensor-sampling")) < 0) return r;
    if ((r = append_string_katcl(l, 0, sensor_name)) < 0) return r;
    if ((r = append_string_katcl(l, KATCP_FLAG_LAST, "auto")) < 0) return r;
    return 0;
}

int request_sensor_fhost_device_status(struct cmc_array *array)
{
    printf("requesting sensors %s...\n", array->name);
    size_t needed = snprintf(NULL, 0, "fhost%02d.device-status", array->host_counter) + 1;
    array->current_sensor_name = malloc(needed);
    sprintf(array->current_sensor_name, "fhost%02d.device-status", array->host_counter);
    ss_append_string_katcl(array->l, array->current_sensor_name);

    return 0;
}

int receive_sensor_fhost_device_status_response(struct cmc_array *array)
{
    if (!strcmp(arg_string_katcl(array->l, 0), "!sensor-sampling") && !strcmp(arg_string_katcl(array->l, 2), array->current_sensor_name))
    {
        // free(array->current_sensor_name); /* need to think of a better place to put this. */
        if (!strcmp(arg_string_katcl(array->l, 1), "ok"))
        {
            printf("sensor-sampling %s ok\n", array->current_sensor_name);
            array->host_counter++;
            return 0;
        }
        else
        {
            printf("sensor-sampling %s failed: %s\n", array->current_sensor_name, arg_string_katcl(array->l, 3));
            return -1;
        }
    }
    else
    {
        printf("unknown katcp message received: %s %s %s %s\n", arg_string_katcl(array->l, 0), arg_string_katcl(array->l, 1), arg_string_katcl(array->l, 2), arg_string_katcl(array->l, 3));
        return -1;
    }
}

void process_sensor_status(struct cmc_array *array)
{
    int host_number;
    char host_type;
    if (sscanf(arg_string_katcl(array->l, 3), "%chost%02d.device-status", &host_type, &host_number) == 2)
    {
        printf("Got %chost%02d.device-status: %s\n", host_type, host_number, arg_string_katcl(array->l, 5));
        if (host_type == 'f')
        {
            sprintf(array->fhosts[host_number]->device_status, "%s", arg_string_katcl(array->l, 4));
        }
        else if (host_type == 'x')
        {
            sprintf(array->fhosts[host_number]->device_status, "%s", arg_string_katcl(array->l, 4));
        }
        else
        {
            printf("I don't know what a %chost is.\n", host_type);
        }
    }
    else 
        printf("Didn't understand what I got: %s %s %s %s %s %s\n", arg_string_katcl(array->l, 0), arg_string_katcl(array->l, 1), arg_string_katcl(array->l, 2), arg_string_katcl(array->l, 3), arg_string_katcl(array->l, 4), arg_string_katcl(array->l, 5));
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

struct fhost *create_fhost(char *hostname, int host_number)
{
    struct fhost *new_fhost = malloc(sizeof(*new_fhost));
    snprintf(new_fhost->hostname, sizeof(new_fhost->hostname), "%s", hostname);
    new_fhost->host_number = host_number;
    return new_fhost;
}

void destroy_fhost(struct fhost *fhost)
{
    free(fhost);
}

struct xhost *create_xhost(char *hostname, int host_number)
{
    struct xhost *new_xhost = malloc(sizeof(*new_xhost));
    snprintf(new_xhost->hostname, sizeof(new_xhost->hostname), "%s", hostname);
    new_xhost->host_number = host_number;
    return new_xhost;
}

void destroy_xhost(struct xhost *xhost)
{
    free(xhost);
}

