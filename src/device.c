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


struct device *device_create(char *new_name)
{
    /*TODO think about sanitising the name*/
    struct device *new_device = malloc(sizeof(*new_device));
    if (new_device != NULL)
    {
        new_device->name = strdup(new_name);
        new_device->number_of_sensors = 0;
        new_device->sensor_list = NULL; /*Making this explicit probably not necessary?*/
    }
    return new_device;
}


void device_destroy(struct device *this_device)
{
    if (this_device != NULL)
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
}


char *device_get_name(struct device *this_device)
{
    return this_device->name;
}


int device_add_sensor(struct device *this_device, char *new_sensor_name)
{
    /*TODO check whether the sensor already exists. Update return values accordingly.*/
    this_device->sensor_list = realloc(this_device->sensor_list, \
            sizeof(*(this_device->sensor_list))*(this_device->number_of_sensors + 1));
    this_device->sensor_list[this_device->number_of_sensors] = sensor_create(new_sensor_name);
    this_device->number_of_sensors++;
    return 0; 
}


char **device_get_sensor_names(struct device *this_device, unsigned int *number_of_sensors)
{
    *number_of_sensors = this_device->number_of_sensors; /*need to return this to the caller*/
    if (this_device->number_of_sensors == 0)
        return NULL; /*Not useful to return a pointer to zero memory space.*/
    char **sensor_names = malloc(sizeof(*sensor_names)*(*number_of_sensors)); 
    int i;
    for (i = 0; i < *number_of_sensors; i++)
    {
        sensor_names[i] = strdup(sensor_get_name(this_device->sensor_list[i]));
    }
    return sensor_names;
}


char *device_get_sensor_value(struct device *this_device, char *sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_device->number_of_sensors; i++)
    {
        if (!strcmp(sensor_name, sensor_get_name(this_device->sensor_list[i])))
        {
            return sensor_get_value(this_device->sensor_list[i]);
        }
    }
    return NULL;
}


char *device_get_sensor_status(struct device *this_device, char *sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_device->number_of_sensors; i++)
    {
        if (!strcmp(sensor_name, sensor_get_name(this_device->sensor_list[i])))
        {
            return sensor_get_status(this_device->sensor_list[i]);
        }
    }
    return NULL;
}


int device_update_sensor(struct device *this_device, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
{
    unsigned int i;
    for (i = 0; i < this_device->number_of_sensors; i++)
    {
        if (!strcmp(sensor_name, sensor_get_name(this_device->sensor_list[i])))
        {
            return sensor_update(this_device->sensor_list[i], new_sensor_value, new_sensor_status);
        }
    }
    return -1;
}


char *device_html_summary(struct device *this_device)
{
    char format[] = "<td><button class=\"%s\">%s</button></td>";
    ssize_t needed = snprintf(NULL, 0, format, device_get_sensor_status(this_device, "device-status"), this_device->name) + 1;
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, device_get_sensor_status(this_device, "device-status"), this_device->name);
    return html_summary;
}


void device_print_sensors(struct device *this_device)
{
    printf("Device name: %s\n", this_device->name);
    unsigned int i;
    for (i = 0; i < this_device->number_of_sensors; i++)
    {
        printf("\t%-15s- %-10s- %-10s\n", sensor_get_name(this_device->sensor_list[i]), sensor_get_value(this_device->sensor_list[i]), sensor_get_status(this_device->sensor_list[i]));
    }
}

