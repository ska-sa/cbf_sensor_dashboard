#include <stdlib.h>
#include <string.h>

#include "sensor.h"

static int host_update_sensor_value_function(struct host *this_host, char *sensor_name, char *new_value)
{
}


struct host *host_create(char type)
{
    struct host *new_host = malloc(sizeof(*new_host));
    if (new_host != NULL)
    {
        new_host->update_sensor_value = host_update_sensor_value_function;
        new_host->host_type = type;
        switch (type)
        {
            case 'f':
                new_host->number_of_sensors = 0;
                add_sensor(new_host, "network");
                break;
            case 'x':
                /* TODO allocate a couple of xengine sensors */
                break;
            default:
                fprintf(stderr, "Unknown host type %c!\n", type);
                free(new_host);
        }
    }
    return new_host;
}
