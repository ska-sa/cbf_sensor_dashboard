#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "host.h"
#include "device.h"
#include "vdevice.h"


struct host {
    char *hostname;
    struct device **device_list;
    unsigned int number_of_devices;
    struct vdevice **vdevice_list;
    unsigned int number_of_vdevices;
    struct host **engine_list;
    unsigned int number_of_engines;
};
    

struct host *host_create()
{
    struct host *new_host = malloc(sizeof(*new_host));
    if (new_host != NULL)
    {
        new_host->hostname = strdup("unknown");
        new_host->number_of_devices = 0;
        new_host->number_of_vdevices = 0;
        new_host->number_of_engines = 0;
    }
    return new_host;
}


void host_destroy(struct host *this_host)
{
    if (this_host != NULL)
    {
        unsigned int i;
        for (i = 0; i < this_host->number_of_vdevices; i++)
            vdevice_destroy(this_host->vdevice_list[i]);
        for (i = 0; i < this_host->number_of_devices; i++)
            device_destroy(this_host->device_list[i]);
        for (i = 0; i < this_host->number_of_engines; i++)
            host_destroy(this_host->engine_list[i]);
        free(this_host);
    }
}


int host_add_device(struct host *this_host, char *new_device_name)
{
    this_host->device_list = realloc(this_host->device_list, \
            sizeof(*(this_host->device_list))*(this_host->number_of_devices + 1));
    this_host->device_list[this_host->number_of_devices] = device_create(new_device_name);
    this_host->number_of_devices++;
    return 0;
}


int host_add_sensor_to_device(struct host *this_host, char *device_name, char *new_sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_host->device_list[i])))
        {
            device_add_sensor(this_host->device_list[i], new_sensor_name);
            return 0;
        }
    }
    return -1;
}


char *host_get_sensor_value(struct host *this_host, char *device_name, char *sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_host->device_list[i])))
        {
            return device_get_sensor_value(this_host->device_list[i], sensor_name);
        }
    }
    return NULL;
}


char *host_get_sensor_status(struct host *this_host, char *device_name, char *sensor_name)
{
    unsigned int i;
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_host->device_list[i])))
        {
            return device_get_sensor_status(this_host->device_list[i], sensor_name);
        }
    }
    if (i == this_host->number_of_devices)
    {
        for (i = 0; i < this_host->number_of_vdevices; i++)
        {
            if (!strcmp(device_name, vdevice_get_name(this_host->vdevice_list[i])))
            {
                return vdevice_get_status(this_host->vdevice_list[i]);
            }
        }
    }
    return NULL;
}


int host_update_sensor(struct host *this_host, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
{
    unsigned int i;
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_host->device_list[i])))
        {
            return device_update_sensor(this_host->device_list[i], sensor_name, new_sensor_value, new_sensor_status);
        }
    }
    return -1;
}
