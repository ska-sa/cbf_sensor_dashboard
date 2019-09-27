#include <stdlib.h>
#include <string.h>

#include "sensor.h"

struct sensor {
    char *name;
    char *value;
    char *status;
};


/**
 * \fn     struct sensor *sensor_create(char *new_name)
 *
 * \detail Allocate memory for a sensor object and populate the members
 *         with sensible default values.
 *
 * \param  new_name A name for the sensor to be created.
 *
 * \return A pointer to the newly-created sensor object.
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


/**
 * \fn     void sensor_destroy(struct sensor *this_sensor)
 *
 * \detail Free the memory associated with the sensor object.
 *
 * \param  this_sensor a pointer to the sensor to be destroyed.
 *
 * \return void
 */
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


/**
 * \fn     char *sensor_get_name(struct sensor *this_sensor)
 *
 * \detail Get the name of the given sensor.
 *
 * \param  this_sensor a pointer to the sensor to be queried.
 *
 * \return A pointer to the name string of the sensor. The char pointer is not newly allocated so therefore does not need to be free'd.
 */
char *sensor_get_name(struct sensor *this_sensor)
{
    return this_sensor->name;
}


/**
 * \fn     char *sensor_get_value(struct sensor *this_sensor)
 *
 * \detail Get the value of the given sensor.
 *
 * \param  this_sensor a pointer to the sensor to be queried.
 *
 * \return A pointer to the value string of the sensor. The char pointer is not newly allocated so therefore does not need to be free'd.
 */
char *sensor_get_value(struct sensor *this_sensor)
{
    return this_sensor->value;
}


/**
 * \fn     char *sensor_get_status(struct sensor *this_sensor)
 *
 * \detail Get the status of the given sensor.
 *
 * \param  this_sensor a pointer to the sensor to be queried.
 *
 * \return A pointer to the status string of the sensor. The char pointer is not newly allocated so therefore does not need to be free'd.
 */
char *sensor_get_status(struct sensor *this_sensor)
{
    return this_sensor->status;
}


/**
 * \fn     int sensor_update(struct sensor *this_sensor, char *new_value, char *new_status)
 *
 * \detail Update the given sensor's value and status.
 *
 * \param  this_sensor a pointer to the sensor to be updated.
 * \param  new_value a string to replace the given sensor's stored sensor value.
 * \param  new_status a string to replace the given sensor's stored operational status.
 *
 * \return An integer indicating the success of the operation.
 */
int sensor_update(struct sensor *this_sensor, char *new_value, char *new_status)
{
    if (this_sensor == NULL)
        return -2; /// \retval -2 The sensor pointer was null - the sensor has not yet been created.
    if (this_sensor->value != NULL)
        free(this_sensor->value);
    if (this_sensor->status != NULL)
        free(this_sensor->status);
    this_sensor->value = strdup(new_value);
    this_sensor->status = strdup(new_status);

    if (this_sensor->value != NULL && this_sensor->status != NULL)
        return 0; /// \retval 0 The update was successful.
    else
        return -1; /// \retval -1 The sensor object exists but its member strings weren't successfully updated.
}


