#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "host.h"
#include "device.h"
#include "vdevice.h"


struct host {
    char *hostname;
    struct device **device_list;
    size_t number_of_devices;
    struct vdevice **vdevice_list;
    size_t number_of_vdevices;
    struct engine **engine_list;
    size_t number_of_engines;
};
    

struct host *host_create()
{
    struct host *new_host = malloc(sizeof(*new_host));
    if (new_host != NULL)
    {
        new_host->hostname = strdup("unknown");
        new_host->number_of_devices = 0;
        new_host->device_list = NULL;
        new_host->number_of_vdevices = 0;
        new_host->vdevice_list = NULL;
        new_host->number_of_engines = 0;
        new_host->engine_list = NULL;
        /*TODO: do I need to explicitly make pointers null?*/
    }
    return new_host;
}


void host_destroy(struct host *this_host)
{
    if (this_host != NULL)
    {
        free(this_host->hostname);
        unsigned int i;
        for (i = 0; i < this_host->number_of_vdevices; i++)
            vdevice_destroy(this_host->vdevice_list[i]);
        free(this_host->vdevice_list);
        for (i = 0; i < this_host->number_of_devices; i++)
            device_destroy(this_host->device_list[i]);
        free(this_host->device_list);
        for (i = 0; i < this_host->number_of_engines; i++)
            engine_destroy(this_host->engine_list[i]);
        free(this_host->engine_list);
        free(this_host);
        this_host = NULL;
    }
}


int host_add_device(struct host *this_host, char *new_device_name)
{
    /*First check whether the device already exists.*/
    int i;
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        if (!strcmp(new_device_name, device_get_name(this_host->device_list[i])))
        {
            return i;
        }
    }
    /*If we got here then clearly it doesnt.*/
    struct device **temp = realloc(this_host->device_list, \
            sizeof(*(this_host->device_list))*(this_host->number_of_devices + 1));
    if (temp != NULL) /*i.e. realloc was successful*/
    {
        this_host->device_list = temp;
        this_host->device_list[this_host->number_of_devices] = device_create(new_device_name);
        this_host->number_of_devices++;
        return (int) this_host->number_of_devices - 1;
    }
    else
        return -1;
}


int host_add_sensor_to_device(struct host *this_host, char *device_name, char *new_sensor_name)
{
    int i;
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_host->device_list[i])))
        {
            return device_add_sensor(this_host->device_list[i], new_sensor_name);
        }
    }
    return -1;
}


int host_add_engine(struct host *this_host, char *new_engine_name)
{
    /*First check whether the engine already exists.*/
    int i;
    for (i = 0; i < this_host->number_of_engines; i++)
    {
        if (!strcmp(new_engine_name, engine_get_name(this_host->engine_list[i])))
        {
            return 1;
        }
    }
    /*Clearly it doesnt.*/
    struct engine **temp = realloc(this_host->engine_list, \
            sizeof(*(this_host->engine_list))*(this_host->number_of_engines + 1));
    if (temp != NULL)
    {
        this_host->engine_list = temp;
        this_host->engine_list[this_host->number_of_engines] = engine_create(new_engine_name);
        this_host->number_of_engines++;
        return (int) this_host->number_of_engines - 1;
    }
    return -1;
}


int host_add_device_to_engine(struct host *this_host, char *engine_name, char *new_device_name)
{
    int i;
    for (i = 0; i < this_host->number_of_engines; i++)
    {
        if (!strcmp(engine_name, engine_get_name(this_host->engine_list[i])))
        {
            return engine_add_device(this_host->engine_list[i], new_device_name);
        }
    }
    return -1;
}


int host_add_sensor_to_engine_device(struct host *this_host, char *engine_name, char *device_name, char *new_sensor_name)
{
    int i;
    int r = -1;
    for (i = 0; i < this_host->number_of_engines; i++)
    {
        if (!strcmp(engine_name, engine_get_name(this_host->engine_list[i])))
        {
            r = engine_add_sensor_to_device(this_host->engine_list[i], device_name, new_sensor_name);
        }
    }
    if (r>=0)
    {
        for (i = 0; i < this_host->number_of_vdevices; i++)
        {
            if (!strcmp(device_name, vdevice_get_name(this_host->vdevice_list[i])))
                break;
        }
        if (i == this_host->number_of_vdevices)
        {
            struct vdevice **temp = realloc(this_host->vdevice_list, \
                    sizeof(*(this_host->vdevice_list))*(this_host->number_of_vdevices + 1));
            if (temp != NULL)
            {
                this_host->vdevice_list = temp;
                this_host->vdevice_list[this_host->number_of_vdevices] = vdevice_create(device_name, &this_host->engine_list, &this_host->number_of_engines);
                this_host->number_of_vdevices++;
            }
        }
    }
    return r;
}


char *host_get_sensor_value(struct host *this_host, char *device_name, char *sensor_name)
{
    int i;
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
    int i;
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
    int i;
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        if (!strcmp(device_name, device_get_name(this_host->device_list[i])))
        {
            return device_update_sensor(this_host->device_list[i], sensor_name, new_sensor_value, new_sensor_status);
        }
    }
    return -1;
}


int host_update_engine_sensor(struct host *this_host, char *engine_name, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
{
    int i;
    for (i = 0; i < this_host->number_of_engines; i++)
    {
        if (!strcmp(engine_name, engine_get_name(this_host->engine_list[i])))
        {
            return engine_update_sensor(this_host->engine_list[i], device_name, sensor_name, new_sensor_value, new_sensor_status);
        }
    }
    return -1;
}


char *host_html_detail(struct host *this_host)
{
    size_t i;
    char *host_detail = strdup(""); //need it to be zero length but don't want it to be null
    /*{
        char format[] = "<!--Host: %s Devices: %u, Engines: %u, Vdevices: %u -->";
        ssize_t needed = snprintf(NULL, 0, format, this_host->hostname, this_host->number_of_devices, this_host->number_of_engines, this_host->number_of_vdevices) + 1;
        host_detail = realloc(host_detail, (size_t) needed);
        sprintf(host_detail, format, this_host->hostname, this_host->number_of_devices, this_host->number_of_engines, this_host->number_of_vdevices);
    }*/ //No longer needed, was just for debugging.
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        char format[] = "%s%s";
        ssize_t needed = snprintf(NULL, 0, format, host_detail, device_html_summary(this_host->device_list[i])) + 1;
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail, format, host_detail, device_html_summary(this_host->device_list[i]));
    }
    for (i = 0; i < this_host->number_of_vdevices; i++)
    {
        char format[] = "%s%s";
        ssize_t needed = snprintf(NULL, 0, format, host_detail, vdevice_html_summary(this_host->vdevice_list[i])) + 1;
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail, format, host_detail, vdevice_html_summary(this_host->vdevice_list[i]));
    }
    return host_detail;
}
