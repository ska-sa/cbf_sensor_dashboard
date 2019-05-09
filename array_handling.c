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
        /* we're not actually going to create the fhosts yet, that is done by the functional mapping */

    int number_of_sensors_per_antenna = 18; /* for now */
    new_array->sensor_names = malloc(sizeof(*(new_array->sensor_names))*new_array->number_of_antennas*number_of_sensors_per_antenna);
    int i;
    for (i = 0; i < new_array->number_of_antennas; i++)
    {
        /* Putting each sensor in its own little scope so that I can reuse variable names. */
        {
            char format[] = "fhost%02d.network.device-status";
            int sensornum = 0; 
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.network.device-status";
            int sensornum = 1;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.spead-rx.device-status";
            int sensornum = 2;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.spead-rx.device-status";
            int sensornum = 3; 
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.network-reorder.device-status";
            int sensornum = 4;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.network-reorder.device-status";
            int sensornum = 5;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.dig.device-status";
            int sensornum = 6;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.spead-tx.device-status";
            int sensornum = 7;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.missing-pkts.device-status";
            int sensornum = 8;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.sync.device-status";
            int sensornum = 9;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.cd.device-status";
            int sensornum = 10;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.pfb.device-status";
            int sensornum = 11;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.quant.device-status";
            int sensornum = 12;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "fhost%02d.ct.device-status";
            int sensornum = 13;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.xeng0.bram-reorder.device-status";
            int sensornum = 14;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.xeng1.bram-reorder.device-status";
            int sensornum = 15;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.xeng2.bram-reorder.device-status";
            int sensornum = 16;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
        {
            char format[] = "xhost%02d.xeng3.bram-reorder.device-status";
            int sensornum = 17;
            size_t needed = snprintf(NULL, 0, format, i) + 1;
            new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum] = malloc(needed);
            sprintf(new_array->sensor_names[i*number_of_sensors_per_antenna + sensornum], format, i);
        }
    }
    new_array->current_sensor = 0;
    new_array->number_of_sensors = new_array->number_of_antennas * number_of_sensors_per_antenna;
    //new_array->current_sensor_name = malloc(1);
    //new_array->current_sensor_name[0] = '\0';
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
    for (i = 0; i < array->number_of_sensors; i++)
        free(array->sensor_names[i]);
    free(array->sensor_names);
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
                char *host_number_str = strndup(arg_string_katcl(array->l, 5) + (i*30 + 26), 2);
                int host_number = atoi(host_number_str);
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
                free(host_number_str);
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
    /*printf("subscribing to sensor %s\n", sensor_name);*/
    int r;
    if ((r = append_string_katcl(l, KATCP_FLAG_FIRST, "?sensor-sampling")) < 0) return r;
    if ((r = append_string_katcl(l, 0, sensor_name)) < 0) return r;
    if ((r = append_string_katcl(l, KATCP_FLAG_LAST, "auto")) < 0) return r;
    return 0;
}

void request_next_sensor(struct cmc_array *array)
{
    printf("requesting sensor %s on %s (number %d of %d)\n", array->sensor_names[array->current_sensor], array->name, array->current_sensor, array->number_of_sensors); 
    ss_append_string_katcl(array->l, array->sensor_names[array->current_sensor]);
    size_t needed = strlen(array->sensor_names[array->current_sensor]) + 1;
    array->current_sensor_name = malloc(needed);
    sprintf(array->current_sensor_name, "%s", array->sensor_names[array->current_sensor]);
}

int receive_next_sensor_ok(struct cmc_array *array)
{
    if (!strcmp(arg_string_katcl(array->l, 0), "!sensor-sampling"))
    {
        if (!strcmp(arg_string_katcl(array->l, 1), "ok"))
        {
            printf("sensor-sampling %s ok (number %d of %d)\n", array->current_sensor_name, array->current_sensor, array->number_of_sensors);
            free(array->current_sensor_name); /* need to think of a better place to put this. */
            array->current_sensor++;
            return 0;
        }
        else
        {
            printf("sensor-sampling %s failed: %s\n", array->current_sensor_name, arg_string_katcl(array->l, 2));
            free(array->current_sensor_name); /* need to think of a better place to put this. */
            array->current_sensor++;
            return -1;
        }
    }
    else
    {
        printf("unknown katcp message received: %s %s %s %s\n", arg_string_katcl(array->l, 0), arg_string_katcl(array->l, 1), arg_string_katcl(array->l, 2), arg_string_katcl(array->l, 3));
        return -2;
    }
}

/*int request_sensor_fhost_device_status(struct cmc_array *array)
{
    printf("requesting sensors %s...\n", array->name);
    char format[] = "fhost%02d.device-status";
    size_t needed = snprintf(NULL, 0, format, array->host_counter) + 1;
    array->current_sensor_name = malloc(needed);
    sprintf(array->current_sensor_name, format, array->host_counter);
    ss_append_string_katcl(array->l, array->current_sensor_name);

    return 0;
}*/

void process_sensor_status(struct cmc_array *array)
{
    int host_number;
    char host_type;
    char *host_info = strtok(arg_string_katcl(array->l, 3), ".");
    char *component_name = strtok(NULL, ".");
    char *sensor_name = strtok(NULL, ".");
    int xeng_number;
    if (sscanf(host_info, "%chost%02d", &host_type, &host_number) == 2)
    {
        if (!strcmp(component_name, "network"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.network.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->netw_rx, "%s", arg_string_katcl(array->l, 4));
                    sprintf(array->fhosts[host_number]->netw_tx, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    sprintf(array->xhosts[host_number]->netw_rx, "%s", arg_string_katcl(array->l, 4));
                    sprintf(array->xhosts[host_number]->netw_tx, "%s", arg_string_katcl(array->l, 4));
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "spead-rx"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.spead-rx.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->spead_rx, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    sprintf(array->xhosts[host_number]->spead_rx, "%s", arg_string_katcl(array->l, 4));
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "network-reorder"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.network-reorder.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->netw_reor, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    sprintf(array->xhosts[host_number]->netw_reor, "%s", arg_string_katcl(array->l, 4));
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "dig"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.dig.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->dig, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    printf("xhost doesn't have a dig sensor???\n");
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "spead-tx"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.spead-tx.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->spead_tx, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    //sprintf(array->xhosts[host_number]->spead_tx, "%s", arg_string_katcl(array->l, 4));
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "missing-pkts"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.missing-pkts.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    printf("fhost doesn't have a missing pkts sensor????\n");
                }
                else if (host_type == 'x')
                {
                    sprintf(array->xhosts[host_number]->miss_pkt, "%s", arg_string_katcl(array->l, 4));
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "sync"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.sync.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->sync, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    printf("xhost doesn't have a sync sensor???\n");
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "cd"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.cd.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->cd, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    printf("xhost doesn't have a cd sensor???\n");
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "pfb"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.pfb.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->pfb, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    printf("xhost doesn't have a pfb sensor???\n");
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "quant"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.quant.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->quant, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    printf("xhost doesn't have a quant sensor???\n");
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (!strcmp(component_name, "ct"))
        {
            if (!strcmp(sensor_name, "device-status"))
            {
                printf("%s: Got %chost%02d.ct.device-status: %s\n", array->name, host_type, host_number, arg_string_katcl(array->l, 5));
                if (host_type == 'f')
                {
                    sprintf(array->fhosts[host_number]->ct, "%s", arg_string_katcl(array->l, 4));
                }
                else if (host_type == 'x')
                {
                    printf("xhost doesn't have a ct sensor???\n");
                }
                else
                {
                    printf("I don't know what a %chost is.\n", host_type);
                }
            }
        }
        else if (sscanf(component_name, "xeng%1d", &xeng_number) == 1)
        {
            char *actual_sensor_name = strtok(NULL, ".");
            if (!strcmp(sensor_name, "bram-reorder"))
            {
                if (!strcmp(actual_sensor_name, "device-status"))
                {
                    switch (xeng_number)
                    {
                        case 0:
                            sprintf(array->xhosts[host_number]->xeng0_bram_reord, "%s", arg_string_katcl(array->l, 4));
                            break;
                        case 1:
                            sprintf(array->xhosts[host_number]->xeng1_bram_reord, "%s", arg_string_katcl(array->l, 4));
                            break;
                        case 2:
                            sprintf(array->xhosts[host_number]->xeng2_bram_reord, "%s", arg_string_katcl(array->l, 4));
                            break;
                        case 3:
                            sprintf(array->xhosts[host_number]->xeng3_bram_reord, "%s", arg_string_katcl(array->l, 4));
                            break;
                        default:
                            fprintf(stderr, "xhost%02d doesn't have an xeng%d!\n", host_number, xeng_number);
                    }
                    /*All of them need to be nominal for the parent to be nominal.*/
                    if (!strcmp(array->xhosts[host_number]->xeng0_bram_reord, "nominal") && !strcmp(array->xhosts[host_number]->xeng1_bram_reord, "nominal") && !strcmp(array->xhosts[host_number]->xeng2_bram_reord, "nominal") && !strcmp(array->xhosts[host_number]->xeng3_bram_reord, "nominal"))
                    {
                        sprintf(array->xhosts[host_number]->bram_reord, "nominal");
                    }
                    if (!strcmp(array->xhosts[host_number]->xeng0_bram_reord, "warn") || !strcmp(array->xhosts[host_number]->xeng1_bram_reord, "warn") || !strcmp(array->xhosts[host_number]->xeng2_bram_reord, "warn") || !strcmp(array->xhosts[host_number]->xeng3_bram_reord, "warn"))
                    {
                        sprintf(array->xhosts[host_number]->bram_reord, "warn");
                    }
                    if (!strcmp(array->xhosts[host_number]->xeng0_bram_reord, "error") || !strcmp(array->xhosts[host_number]->xeng1_bram_reord, "error") || !strcmp(array->xhosts[host_number]->xeng2_bram_reord, "error") || !strcmp(array->xhosts[host_number]->xeng3_bram_reord, "error"))
                    {
                        sprintf(array->xhosts[host_number]->bram_reord, "error");
                    }
                    if (!strcmp(array->xhosts[host_number]->xeng0_bram_reord, "unknown") || !strcmp(array->xhosts[host_number]->xeng1_bram_reord, "unknown") || !strcmp(array->xhosts[host_number]->xeng2_bram_reord, "unknown") || !strcmp(array->xhosts[host_number]->xeng3_bram_reord, "unknown"))
                    {
                        sprintf(array->xhosts[host_number]->bram_reord, "unknown");
                    }
                }
            }
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
    memset(new_fhost, 0, sizeof(*new_fhost)); /*make sure no funny stuff comes up later*/
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
    memset(new_xhost, 0, sizeof(*new_xhost)); /*make sure no funny stuff comes up later*/
    snprintf(new_xhost->hostname, sizeof(new_xhost->hostname), "%s", hostname);
    new_xhost->host_number = host_number;
    sprintf(new_xhost->spead_tx, "unknown");
    return new_xhost;
}

void destroy_xhost(struct xhost *xhost)
{
    free(xhost);
}

