#ifndef _SENSOR_H_
#define _SENSOR_H_
#include <time.h>

/**
 * \file   sensor.h
 * \brief  The sensor type stores the name, value and status of a sensor.
 *         It is meant to be a member of a device object.
 */

struct sensor;

struct sensor *sensor_create(char *new_name);
void sensor_destroy(struct sensor *this_sensor);
char *sensor_get_name(struct sensor *this_sensor);
char *sensor_get_value(struct sensor *this_sensor);
char *sensor_get_status(struct sensor *this_sensor);
int sensor_update(struct sensor *this_sensor, char *new_value, char *new_status);
time_t sensor_get_last_updated(struct sensor *this_sensor);

#endif

