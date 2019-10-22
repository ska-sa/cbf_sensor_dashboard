#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "device.h"
#include "sensor.h"

/// A struct to represent a device - a collection of related sensors in the corr2_sensor_servelet.
struct device {
    /// Name of the device.
    char *name;
    /// Pointer to a list of sensors.
    struct sensor **sensor_list;
    /// Number of sensors in the list.
    unsigned int number_of_sensors;
};


/**
 * \fn      struct device *device_create(char *new_name)
 * \details Allocate memory for a device object, populate the members
 *          with sensible default values.
 * \param   new_name A name for the device to be created.
 * \return  A pointer to the newly-created device object.
 */
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


/**
 * \fn      void device_destroy(struct device *this_device)
 * \details Free the memory associated with the device object. Also desetroy
 *          the underlying sensor objects.
 * \param   this_device A pointer to the device to be destroyed.
 * \return  void
 */
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


/**
 * \fn      char *device_get_name(struct device *this_device)
 * \details Get the name of the given device.
 * \param   this_device A pointer to the device to be queried.
 * \return  A pointer to the name string of the device. The char pointer
 *          is not newly allocated and therefore must not be free'd.
 */
char *device_get_name(struct device *this_device)
{
    return this_device->name;
}


/**
 * \fn      int device_add_sensor(struct device *this_device, char *new_sensor_name)
 * \details Add a sensor to the given device.
 * \param   this_device A pointer to the device to which a sensor will be added.
 * \param   new_sensor_name A char* string with the name of the new sensor.
 * \return  An integer indicating the success of the operation.
 */
int device_add_sensor(struct device *this_device, char *new_sensor_name)
{
    /// TODO check whether the sensor already exists. Update return values accordingly.
    this_device->sensor_list = realloc(this_device->sensor_list, \
            sizeof(*(this_device->sensor_list))*(this_device->number_of_sensors + 1));
    this_device->sensor_list[this_device->number_of_sensors] = sensor_create(new_sensor_name);
    this_device->number_of_sensors++;
    return 0;  /// \retval 0 The sensor was successfully created and a device added.
    /// TODO Figure out how to indicate failure.
}


/**
 * \fn      char **device_get_sensor_names(struct device *this_device, unsigned int *number_of_sensors)
 * \details Get a list of names of the device's sensor members.
 * \param   this_device A pointer to the device.
 * \param   number_of_sensors A pointer to an integer so that the function can return the number
 *          of sensors in the list.
 * \return  A pointer to an array of strings containing the names of the device's sensors.
 */
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


/**
 * \fn      char *device_get_sensor_value(struct sensor *this_device, char *sensor_name)
 * \details Query the device for the value of one of its sensors.
 * \param   this_device A pointer to the device to be queried.
 * \param   sensor_name A char* contianing the name of the sensor to be queried.
 * \return  A pointer to the value string of the sensor. The char* is
 *          not newly allocated so therefore must not be free'd.
 */
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
    return NULL; /// \retval NULL In the event that the sensor is not found.
}


/**
 * \fn      char *device_get_sensor_status(struct device *this_device, char *sensor_name)
 * \details Query the device for the status of one of its sensors.
 * \param   this_device A pointer to the device to be queried.
 * \param   sensor_name A char* contianing the name of the sensor to be queried.
 * \return  A pointer to the status string of the sensor. The char* is
 *          not newly allocated so therefore must not be free'd.
 */
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


/**
 * \fn      int device_update_sensor(struct device *this_device, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
 * \details Update the value and status of the named sensor underneath the given device.
 * \param   this_device A pointer to the device.
 * \param   sensor_name A string containing the name of the sensor to be updated.
 * \param   new_sensor_value A string to replace the named sensor's stored sensor value
 * \param   new_sensor_status A string to replace the named sensor's stored operational status.
 * \return  An integer indicating the success of the operation.
 */
int device_update_sensor(struct device *this_device, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
{
    unsigned int i;
    for (i = 0; i < this_device->number_of_sensors; i++)
    {
        if (!strcmp(sensor_name, sensor_get_name(this_device->sensor_list[i])))
        {
            return sensor_update(this_device->sensor_list[i], new_sensor_value, new_sensor_status); 
            /// \retval 0 The operation was successful.
        }
    }
    return -1; /// \retval -1 The operation failed.
}


/**
 * \fn      char *device_html_summary(struct device *this_device)
 * \details Get an HTML summary of the device. This is an HTML5 td with the class set to the status of the
 *          "device-status" sensor, so that the higher-level CSS can render the button appropriately.
 * \param   this_device A pointer to the device.
 * \return  A newly allocated string containing the HTML summary of the device.
 */
char *device_html_summary(struct device *this_device)
{
    char format[] = "<td class=\"%s\">%s</td>";
    /// TODO some kind of check in case the device doens't have a "device-status" sensor.
    ssize_t needed = snprintf(NULL, 0, format, device_get_sensor_status(this_device, "device-status"), this_device->name) + 1;
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, device_get_sensor_status(this_device, "device-status"), this_device->name);
    return html_summary;
}
