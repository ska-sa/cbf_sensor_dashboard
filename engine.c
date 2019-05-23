#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "engine.h"
#include "device.h"
#include "sensor.h"

struct engine {
    char *name;
    struct device **device_list;
    unsigned int number_of_devices;
};


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


void engine_destroy(struct engine *this_engine)
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


char *engine_get_name(struct engine *this_engine)
{
    return this_engine->name;
}


int engine_add_device(struct engine *this_engine, char *new_device_name)
{
    this_engine->device_list = realloc(this_engine->device_list, \
            sizeof(*(this_engine->device_list))*(this_engine->number_of_devices + 1));
    this_engine->device_list[this_engine->number_of_devices] = device_create(new_device_name);
    this_engine->number_of_devices++;
    return 0;
}

int engine_add_sensor_to_device(struct engine *this_engine, char *device_name, char *new_sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_engine->device_list[i])))
        {
            device_add_sensor(this_engine->device_list[i], new_sensor_name);
            return 0;
        }
    }
    return -1;
}


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


void engine_print(struct engine *this_engine)
{
    printf("Engine: %s\n", engine_get_name(this_engine));
    unsigned int i;
    for (i = 0; i < this_engine->number_of_devices; i++)
    {
        device_print_sensors(this_engine->device_list[i]);
    }
}
