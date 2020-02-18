#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>
#include <stddef.h>

#include "host.h"
#include "device.h"
#include "vdevice.h"


/// A struct to represent an FPGA host, which has some devices and engines on it.
struct host {
    /// The serial number of the FPGA host.
    char *host_serial;
    /// The type of gateware present on the host (i.e. 'f' or 'x').
    char type;
    /// The index of the host in its team.
    int host_number;
    /// The name of the input stream - nominally this should represent which antenna or dummy input is being given to an fhost.
    char *host_input_stream_name;
    /// The list of devices that are on the host.
    struct device **device_list;
    /// The number of devices in the list.
    size_t number_of_devices;
    /// The list of vdevices that are ont he host.
    struct vdevice **vdevice_list;
    /// The number of vdevices in the list.
    size_t number_of_vdevices;
    /// The list of engines that are on the device.
    struct engine **engine_list;
    /// The number of engines in the list.
    size_t number_of_engines;
};
    

/**
 * \fn      struct host *host_create(char type, int host_number)
 * \details Allocate memory for a new host object, initialise members with values indicating that it hasn't got any details yet.
 * \param   type The type ('f' or 'x') of host to create.
 * \param   host_number The host's index in its team. It needs to know this.
 * \return  A pointer to the newly-allocated host.
 */
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


/**
 * \fn      void host_destroy(struct host *this_host)
 * \details Free the memory allocated to this host, and its child objects.
 * \param   this_host A pointer to the host in question.
 * \return  void
 */
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


/**
 * \fn      int host_set_serial_no(struct host *this_host, char *host_serial)
 * \details Set the serial number of the host object.
 * \param   this_host A pointer to the host in question.
 * \param   host_serial A string containing the serial number to be set.
 * \return  At the moment, this function will always return 1 to indicate success. No error checking is performed.
 */
int host_set_serial_no(struct host *this_host, char *host_serial)
{
    free(this_host->host_serial);
    this_host->host_serial = strdup(host_serial);
    return 1;
}


/**
 * \fn      int host_add_device(struct host *this_host, char *new_device_name) 
 * \details Add a device to the host.
 * \param   this_host A pointer to the host in question.
 * \param   new_device_name A string containing the name of the device to be created.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      int host_add_sensor_to_device(struct host *this_host, char *device_name, char *new_sensor_name)
 * \details Add a sensor to a device on the host.
 * \param   this_host A pointer to the host in question.
 * \param   device_name The name of the device which will get a sensor added.
 * \param   new_sensor_name The name of the new sensor to be added to the device.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      int host_add_engine(struct host *this_host, char *new_engine_name)
 * \details Add an engine to the host.
 * \param   this_host A pointer to the host in question.
 * \param   new_engine_name A string containing the intended new name for the engine.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      int host_add_device_to_engine(struct host *this_host, char *engine_name, char *new_device_name)
 * \details Add a device to an engine on the host.
 * \param   this_host A pointer to the host in question.
 * \param   engine_name A string containing the name of the engine which needs a device added.
 * \param   new_device_name A string containing the intended name for the new device.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      int host_add_sensor_to_engine_device(struct host *this_host, char *engine_name, char *device_name, char *new_sensor_name)
 * \details Add a sensor to a device on an engine on the host.
 * \param   this_host A pointer to the host in question.
 * \param   engine_name A string containing the name of the engine which needs a sensor added to one of its devices.
 * \param   device_name A string containing the name of the device to which a sensor needs to be added.
 * \param   new_sensor_name A string contianing the intended name for the new sensor.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      int host_update_input_stream(struct host *this_host, char *new_input_stream_name)
 * \details Set the input stream name for the host.
 * \param   this_host A pointer to the host in question.
 * \param   new_input_stream_name A string containing the intended name of the input stream.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      char *host_get_input_stream(struct host *this_host)
 * \details Get the input-stream name (normally the MeerKAT antenna name) going to the host.
 *          Mostly applicable to fhosts.
 * \param   this_host A pointer to the host in question.
 * \return  A string containing the name of the host's input stream. This MUST NOT be free()'d elsewhere.
 */
char *host_get_input_stream(struct host *this_host)
{
    return this_host->host_input_stream_name;
}


/**
 * \fn      char *host_get_sensor_value(struct host *this_host, char *device_name, char *sensor_name)
 * \details Get the value from a sensor on the host.
 * \param   this_host A pointer to the host in question.
 * \param   device_name A string with the name of the device which contains the desired sensor.
 * \param   sensor_name A string contianing the name of the sensor to be retrieved.
 * \return  A string containing the sensor value for the desired sensor. This is not newly allocated and must
 *          not be freed. NULL if the process failed.
 */
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


/**
 * \fn      char *host_get_sensor_status(struct host *this_host, char *device_name, char *sensor_name)
 * \details Get the status from a sensor on the host.
 * \param   this_host A pointer to the host in question.
 * \param   device_name A string with the name of the device which contains the desired sensor.
 * \param   sensor_name A string contianing the name of the sensor to be retrieved.
 * \return  A string containing the sensor status for the desired sensor. This is not newly allocated and must
 *          not be freed. NULL if the process failed.
 */
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


/**
 * \fn      int host_update_sensor(struct host *this_host, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
 * \details Update the value and status of one of the sensors on the host.
 * \param   this_host A pointer to the host in question.
 * \param   device_name A string with the name of the device which contains the desired sensor.
 * \param   sensor_name A string contianing the name of the sensor to be updated.
 * \param   new_sensor_value A string containing the new value to store in the sensor.
 * \param   new_sensor_status A string containing the new status to store in the sensor.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      int host_update_engine_sensor(struct host *this_host, char *engine_name, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
 * \details Update the value and status of one of the sensors in one of the host's engines.
 * \param   this_host A pointer to the host in question.
 * \param   engine_name A string with the name of the engine which contains the desired device.
 * \param   device_name A string with the name of the device which contains the desired sensor.
 * \param   sensor_name A string contianing the name of the sensor to be updated.
 * \param   new_sensor_value A string containing the new value to store in the sensor.
 * \param   new_sensor_status A string containing the new status to store in the sensor.
 * \return  An integer indicating the outcome of the operation.
 */
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


/**
 * \fn      char *host_html_detail(struct host *this_host)
 * \details Get an HTML description of the host by concatenating HTML descriptions of the underlying devices and vdevices in the host.
 * \param   this_host A pointer to the host in question.
 * \return  A newly allocated string containing the HTML description of the host.
 */
char *host_html_detail(struct host *this_host)
{
    size_t i;
    char *host_detail = strdup(""); //need it to be zero length but don't want it to be null

    if (this_host->host_input_stream_name != NULL)
    {
        char format[] = "<td style=\"width: 1%%\">%s</td>";
        ssize_t needed = snprintf(NULL, 0, format, this_host->host_input_stream_name) + 1;
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail, format, this_host->host_input_stream_name);
    }
    {
        char format[] = "<td>%c%d %s</td>";
        ssize_t needed = snprintf(NULL, 0, format, this_host->type, this_host->host_number, this_host->host_serial) + 1;
        needed += (ssize_t) strlen(host_detail);
        host_detail = realloc(host_detail, (size_t) needed); //TODO checks for errors.
        sprintf(host_detail + strlen(host_detail), format, this_host->type, this_host->host_number, this_host->host_serial);
    }
    for (i = 0; i < this_host->number_of_devices; i++)
    {
        char *dev_html_summ = device_html_summary(this_host->device_list[i]);
        size_t needed = strlen(host_detail) + strlen(dev_html_summ) + 1;
        host_detail = realloc(host_detail, needed);
        strcat(host_detail, dev_html_summ);
        free(dev_html_summ);
    }
    for (i = 0; i < this_host->number_of_vdevices; i++)
    {
        char *vdev_html_summ = vdevice_html_summary(this_host->vdevice_list[i]);
        host_detail = realloc(host_detail, strlen(host_detail) + strlen(vdev_html_summ) + 1);
        strcat(host_detail, vdev_html_summ);
        free(vdev_html_summ);
    }
    return host_detail;
}
