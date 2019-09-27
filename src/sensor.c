#include <stdlib.h>
#include <string.h>

#include "sensor.h"

struct sensor {
    char *name;
    char *value;
    char *status;
};

/**
 * \brief  Create a sensor object.
 *
 * \detail Allocate memory for a sensor object and populate the members
 *         with sensible default values.
 *
 */
struct sensor *sensor_create(char *new_name)
{
    /* TODO think about sanitising the name a bit perhaps. */
    struct sensor *new_sensor = malloc(sizeof(*new_sensor));
    if (new_sensor != NULL)
    {
        new_sensor->name = strdup(new_name);
        new_sensor->value = strdup("unused");
        new_sensor->status = strdup("unknown");
    }
    return new_sensor;
}


void sensor_destroy(struct sensor *this_sensor)
{
    if (this_sensor != NULL)
    {
        free(this_sensor->name);
        free(this_sensor->value);
        free(this_sensor->status);
        free(this_sensor);
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


char *sensor_get_status(struct sensor *this_sensor)
{
    return this_sensor->status;
}


int sensor_update(struct sensor *this_sensor, char *new_value, char *new_status)
{
    if (this_sensor == NULL)
        return -2;
    if (this_sensor->value != NULL)
        free(this_sensor->value);
    if (this_sensor->status != NULL)
        free(this_sensor->status);
    this_sensor->value = strdup(new_value);
    this_sensor->status = strdup(new_status);

    if (this_sensor->value != NULL && this_sensor->status != NULL)
        return 0;
    else
        return -1;
}


