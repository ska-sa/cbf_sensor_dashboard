#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <syslog.h>

#include "engine.h"
#include "device.h"
#include "sensor.h"

/// A struct to represent an engine on a host.
struct engine {
    /// The engine's name.
    char *name;
    /// A list of devices in the engine.
    struct device **device_list;
    /// The number of devices in the list.
    unsigned int number_of_devices;
};


/**
 * \fn      struct engine *engine_create(char *new_name)
 * \details Allocate memory for a new engine object, populate the members with NULLs.
 * \param   new_name A string containing the name for the new engine object.
 * \return  A pointer to the newly-allocated engine object.
 */
struct engine *engine_create(char *new_name)
{
    struct engine *new_engine = malloc(sizeof(*new_engine));
    if (new_engine != NULL)
    {
        new_engine->name = strdup(new_name);
        new_engine->device_list = NULL;
        new_engine->number_of_devices = 0;
    }
    return new_engine;
}


/**
 * \fn      void engine_destroy(struct engine *this_engine)
 * \details Free the memory allocated to the engine object, and all its children.
 * \param   this_engine A pointer to the engine to be destroyed.
 * \return  void
 */
void engine_destroy(struct engine *this_engine)
{
    if (this_engine != NULL)
    {
        if (this_engine->name != NULL)
        {
            free(this_engine->name);
            unsigned int i;
            for (i = 0; i < this_engine->number_of_devices; i++)
            {
                device_destroy(this_engine->device_list[i]);
            }
            free(this_engine->device_list);
        }
        free(this_engine);
    }
}


/**
 * \fn      char *engine_get_name(struct engine *this_engine)
 * \details Get the name of the given engine.
 * \param   this_engine A pointer to the engine in question.
 * \return  A string containing the name of the engine.
 */
char *engine_get_name(struct engine *this_engine)
{
    return this_engine->name;
}


/**
 * \fn      int engine_add_device(struct engine *this_engine, char *new_device_name)
 * \details Add a device to the engine.
 * \param   this_engine A pointer to the engine in question.
 * \param   new_device_name A string containing the intended name for the new device.
 * \return  An integer indicating the outcome of the operation. At the moment
 *          this function should always return zero, indicating success.
 */
int engine_add_device(struct engine *this_engine, char *new_device_name)
{
    /*TODO make it return -1 if there's a failure.*/
    this_engine->device_list = realloc(this_engine->device_list, \
            sizeof(*(this_engine->device_list))*(this_engine->number_of_devices + 1));
    this_engine->device_list[this_engine->number_of_devices] = device_create(new_device_name);
    this_engine->number_of_devices++;
    return 0;
}

/**
 * \fn      int engine_add_sensor_to_device(struct engine *this_engine, char *device_name, char *new_sensor_name)
 * \details Add a sensor to an existing device on the engine.
 * \param   this_engine A pointer to the engine in question.
 * \param   device_name A string containing the name of the device to which the sensor should be added.
 *                      This device should already exist, or the function will return indicating failure.
 * \param   new_sensor_name A string containing the intended name for the sensor to be created.
 * \return  An integer indicating the outcome of the operation.
 */
int engine_add_sensor_to_device(struct engine *this_engine, char *device_name, char *new_sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_engine->device_list[i])))
        {
            device_add_sensor(this_engine->device_list[i], new_sensor_name);
            return 0; //TODO device_add_sensor doesn't guarantee success - check return value.
                      // Might be easier just to return the device_add_sensor's result directly?
        }
    }
    return -1;
}


/**
 * \fn      char *engine_get_sensor_value(struct engine *this_engine, char *device_name, char *sensor_name)
 * \details Retrieve the sensor value from a sensor on one of the engine's devices.
 * \param   this_engine A pointer to the engine in question.
 * \param   device_name A string containing the name of the device to be queried.
 * \param   sensor_name A string containing the name of the sensor to be queried.
 * \return  A string containing the sensor's value. This is not newly allocated, and must not be freed.
 *          Returns NULL if the operation was unsuccessful.
 */
char *engine_get_sensor_value(struct engine *this_engine, char *device_name, char *sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_engine->device_list[i])))
        {
            return device_get_sensor_value(this_engine->device_list[i], sensor_name);
        }
    }
    return NULL;
}


/**
 * \fn      char *engine_get_sensor_status(struct engine *this_engine, char *device_name, char *sensor_name)
 * \details Retrieve the sensor status from a sensor on one of the engine's devices.
 * \param   this_engine A pointer to the engine in question.
 * \param   device_name A string containing the name of the device to be queried.
 * \param   sensor_name A string containing the name of the sensor to be queried.
 * \return  A string containing the sensor's status. This is not newly allocated, and must not be freed.
 *          Returns NULL if the operation was unsuccessful.
 */
char *engine_get_sensor_status(struct engine *this_engine, char *device_name, char *sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_engine->device_list[i])))
        {
            return device_get_sensor_status(this_engine->device_list[i], sensor_name);
        }
    }
    return NULL;
}


/**
 * \fn      time_t engine_get_sensor_time(struct engine *this_engine, char *device_name, char *sensor_name)
 * \details Retrieve the last updated time from a sensor on one of the engine's devices.
 * \param   this_engine A pointer to the engine in question.
 * \param   device_name A string containing the name of the device to be queried.
 * \param   sensor_name A string containing the name of the sensor to be queried.
 * \return  The time in seconds of the sensor's last update.
 *          Returns NULL if the operation was unsuccessful.
 */
time_t engine_get_sensor_time(struct engine *this_engine, char *device_name, char *sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_engine->device_list[i])))
        {
            return device_get_sensor_time(this_engine->device_list[i], sensor_name);
        }
    }
    return 0;
}


/**
 * \fn      int engine_update_sensor(struct engine *this_engine, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
 * \details Write a new value and status to a sensor on one of the engine's devices.
 * \param   this_engine A pointer to the engine in question.
 * \param   device_name A string containing the name of the device containing the intended sensor.
 * \param   sensor_name A string containing the name of the sensor to be updated.
 * \param   new_sensor_value A string containing the new value to be written to the sensor.
 * \param   new_sensor_status A string contianing the new status to be written to the sensor.
 * \return  An integer indicating the outcome of the operation.
 */
int engine_update_sensor(struct engine *this_engine, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
{
    unsigned int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_engine->device_list[i])))
        {
            return device_update_sensor(this_engine->device_list[i], sensor_name, new_sensor_value, new_sensor_status);
        }
    }
    return -1;
}


/**
 * \fn      char** engine_get_stagnant_sensor_names(struct engine *this_engine, time_t stagnant_time, size_t *number_of_sensors)
 * \details Get a list of names of the engine's sensors (via its child devices) which haven't been updated for a specified amount of time.
 * \param   this_engine A pointer to the engine.
 * \param   stagnant_time The time in seconds above which sensors should be reported stagnant.
 * \param   number_of_sensors A pointer to an integer so that the function can return the number of sensors in the list.
 * \return  A pointer to an array of strings containing the names of the engine's stagnant sensors.
 */
char** engine_get_stagnant_sensor_names(struct engine *this_engine, time_t stagnant_time, size_t *number_of_sensors)
{
    *number_of_sensors = 0;

    char **sensor_names = NULL;
    int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        size_t batch_n_sensors = 0;
        char **batch_sensor_names = device_get_stagnant_sensor_names(this_engine->device_list[i], stagnant_time, &batch_n_sensors);
        sensor_names = realloc(sensor_names, sizeof(*sensor_names)*(*number_of_sensors + batch_n_sensors));
        int j;
        for (j = 0; j < batch_n_sensors; j++)
        {
            ssize_t needed = snprintf(NULL, 0, "%s.%s", this_engine->name, batch_sensor_names[j]) + 1;
            sensor_names[*number_of_sensors + (size_t) j] = malloc((size_t) needed);
            sprintf(sensor_names[*number_of_sensors + (size_t) j], "%s.%s", this_engine->name, batch_sensor_names[j]);
            free(batch_sensor_names[j]);
        }
        free(batch_sensor_names);
        *number_of_sensors += batch_n_sensors;
    }
    if (*number_of_sensors)
        syslog(LOG_DEBUG, "Engine %s reported %ld stagnant sensor%s.", this_engine->name, *number_of_sensors, *number_of_sensors == 1 ? "" : "s");
    return sensor_names;
}


