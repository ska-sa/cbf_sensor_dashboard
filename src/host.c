#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "host.h"
#include "device.h"
#include "vdevice.h"


struct host {
    char *host_serial;
    char type;
    int host_number;
    char *host_input_stream_name;
    struct device **device_list;
    size_t number_of_devices;
    struct vdevice **vdevice_list;
    size_t number_of_vdevices;
    struct engine **engine_list;
    size_t number_of_engines;
};
    

struct host *host_create(char type, int host_number)
{
    struct host *new_host = malloc(sizeof(*new_host));
    if (new_host != NULL)
    {
        new_host->host_serial = strdup("unknwn");
        new_host->type = type;
        new_host->host_number = host_number;
        new_host->host_input_stream_name = NULL;
        new_host->number_of_devices = 0;
        new_host->device_list = NULL;
        new_host->number_of_vdevices = 0;
        new_host->vdevice_list = NULL;
        new_host->number_of_engines = 0;
        new_host->engine_list = NULL;
    }
    return new_host;
}


void host_destroy(struct host *this_host)
{
    if (this_host != NULL)
    {
        free(this_host->host_serial);
        if (this_host->host_input_stream_name)
            free(this_host->host_input_stream_name);
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


int host_set_serial_no(struct host *this_host, char *host_serial)
{
    free(this_host->host_serial);
    this_host->host_serial = strdup(host_serial);
    return 1;
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


int host_update_input_stream(struct host *this_host, char *new_input_stream_name)
{
    if (new_input_stream_name != NULL)
    {
        //syslog(LOG_INFO, "%chost%02d receiving input %s.", this_host->type, this_host->host_number, new_input_stream_name);
        this_host->host_input_stream_name = strdup(new_input_stream_name);
        size_t last_char = strlen(this_host->host_input_stream_name) - 1;
        //trim the polarisation off the end. We don't need to know that.
        if (this_host->host_input_stream_name[last_char] == 'h' || this_host->host_input_stream_name[last_char] == 'v')
            this_host->host_input_stream_name[last_char] = '\0';
        return 0;
    }
    return -1;
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

    if (this_host->host_input_stream_name != NULL)
    {
        char format[] = "<td style=\"width: 1\%\">%s</td>";
        ssize_t needed = snprintf(NULL, 0, format, this_host->host_input_stream_name) + 1;
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail, format, this_host->host_input_stream_name);
    }
    {
        char format[] = "%s<td>%c%d %s</td>";
        ssize_t needed = snprintf(NULL, 0, format, host_detail, this_host->type, this_host->host_number, this_host->host_serial) + 1;
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail, format, host_detail, this_host->type, this_host->host_number, this_host->host_serial);
    }
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        char format[] = "%s%s";
        char *dev_html_summ = device_html_summary(this_host->device_list[i]);
        ssize_t needed = snprintf(NULL, 0, format, host_detail, dev_html_summ) + 1;
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail, format, host_detail, dev_html_summ);
        free(dev_html_summ);
    }
    for (i = 0; i < this_host->number_of_vdevices; i++)
    {
        char format[] = "%s%s";
        char *vdev_html_summ = vdevice_html_summary(this_host->vdevice_list[i]);
        ssize_t needed = snprintf(NULL, 0, format, host_detail, vdev_html_summ) + 1;
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail, format, host_detail, vdev_html_summ);
        free(vdev_html_summ);
    }
    return host_detail;
}
