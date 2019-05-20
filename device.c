#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "device.h"
#include "sensor.h"

struct device {
    char *name;
    struct sensor **sensor_list;
    unsigned int number_of_sensors;
};


struct device *device_create(char *new_name, char **sensor_names, unsigned int number_of_sensors)
{
    struct device *new_device = malloc(sizeof(*new_device));
    if (new_device != NULL)
    {
        new_device->name = strdup(new_name);
        new_device->sensor_list = malloc(sizeof(*(new_device->sensor_list))*number_of_sensors);
        unsigned int i;
        for (i = 0; i < number_of_sensors; i++)
        {
            new_device->sensor_list[i] = sensor_create(sensor_names[i]);
        }
        new_device->number_of_sensors = number_of_sensors;
    }
    return new_device;
}

void device_destroy(struct device *this_device)
{
    if (this_device->name != NULL)
    {
        free(this_device->name);
        unsigned int i;
        for (i = 0; i < this_device->number_of_sensors; i++)
        {
            sensor_destroy(this_device->sensor_list[i]);
        }
        free(this_device->sensor_list);
    }
    free(this_device);
}

char *device_get_name(struct device *this_device)
{
    return this_device->name;
}

void device_print_sensors(struct device *this_device)
{
    printf("Device name: %s\n", this_device->name);
    unsigned int i;
    for (i = 0; i < this_device->number_of_sensors; i++)
    {
        printf("\t%-15s- %s\n", sensor_get_name(this_device->sensor_list[i]), sensor_get_value(this_device->sensor_list[i]));
    }
}

