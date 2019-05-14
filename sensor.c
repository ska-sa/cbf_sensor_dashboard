#include <stdlib.h>
#include <string.h>

#include "sensor.h"


static int sensor_update_value_function(struct sensor *this_sensor, char *new_value)
{
    if (this_sensor->value != NULL)
        free(this_sensor->value);
    this_sensor->value = strdup(new_value);
    if (this_sensor->value != NULL)
        return 0;
    else
        return -1;
}


struct sensor *create_sensor(char *name)
{
    /* TODO think about sanitising the name a bit perhaps. */
    struct sensor *new_sensor = malloc(sizeof(*new_sensor));
    if (new_sensor != NULL)
    {
        new_sensor->name = strdup(name);
        new_sensor->update_value = sensor_update_value_function;
    }
    return new_sensor;
}


void destroy_sensor(struct sensor *the_sensor)
{
    if (the_sensor->name != NULL)
        free(the_sensor->name);
    if (the_sensor != NULL)
        free(the_sensor);
}

