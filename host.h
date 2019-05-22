#ifdef _HOST_H_
#define _HOST_H_

#include "sensor.h"

struct host;

struct host *host_create(char type);
void host_destroy(struct host *this_host);

int host_add_device(struct host *this_host, char *new_device_name);
int host_add_sensor_to_device(struct host *this_host, char *device_name, char *new_sensor_name);
char *host_get_sensor_value(struct host *this_host, char *device_name, char *sensor_name);
char *host_get_sensor_status(struct host *this_host, char *device_name, char *sensor_name);
int host_update_sensor(struct host *this_host, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status);
#endif

