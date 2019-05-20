#ifndef _SENSOR_H_
#define _SENSOR_H_

struct sensor;

struct sensor *sensor_create(char *new_name);
void sensor_destroy(struct sensor *this_sensor);
char *sensor_get_name(struct sensor *this_sensor);
char *sensor_get_value(struct sensor *this_sensor);
int sensor_update_value(struct sensor *this_sensor, char *new_value);

#endif

