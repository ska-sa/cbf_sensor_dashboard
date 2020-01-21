#ifndef _DEVICE_H_
#define _DEVICE_H_
#include <time.h>

/**
 * \file   device.h
 * \brief  The device type stores the status of a device on the FPGA reported by the corr2_sensor_servelet.
 *         The device collects a number of specific sensors, but its overall status is defined by a
 *         "device-status" sensor.
 */

struct device;

struct device *device_create(char *new_name);
void device_destroy(struct device *this_device);
char *device_get_name(struct device *this_device);
int device_add_sensor(struct device *this_device, char *new_sensor_name); /* I don't think we need the capability to remove sensors for the time being. */
char *device_get_sensor_value(struct device *this_device, char *sensor_name);
char *device_get_sensor_status(struct device *this_device, char *sensor_name);
int device_update_sensor(struct device *this_device, char *sensor_name, char *new_sensor_value, char *new_sensor_status);

char *device_html_summary(struct device *this_device);

#endif
