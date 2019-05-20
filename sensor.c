#include <stdlib.h>
#include <string.h>

#include "sensor.h"

struct sensor {
    char *name;
    char *value;
};


struct compound_sensor {
    char *engine_name;
    char *name;
    struct sensor **sensor_list;
    char *value;
};


struct sensor *sensor_create(char *new_name)
{
    /* TODO think about sanitising the name a bit perhaps. */
    struct sensor *new_sensor = malloc(sizeof(*new_sensor));
    if (new_sensor != NULL)
    {
        new_sensor->name = strdup(new_name);
        new_sensor->value = strdup("unused");
    }
    return new_sensor;
}


void sensor_destroy(struct sensor *the_sensor)
{
    if (the_sensor != NULL)
    {
        free(the_sensor->name);
        free(the_sensor->value);
        free(the_sensor);
    }
}


char *sensor_get_name(struct sensor *this_sensor)
{
    return this_sensor->name;
}


char *sensor_get_value(struct sensor *this_sensor)
{
    return this_sensor->value;
}


int sensor_update_value(struct sensor *this_sensor, char *new_value)
{
    if (this_sensor->value != NULL)
        free(this_sensor->value);
    this_sensor->value = strdup(new_value);
    if (this_sensor->value != NULL)
        return 0;
    else
        return -1;
}


