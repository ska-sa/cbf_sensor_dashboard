#ifndef _HOST_H_
#define _HOST_H_

struct host;

struct host *host_create(char type, int host_number);
void host_destroy(struct host *this_host);

int host_set_serial_no(struct host *this_host, char *host_serial);

int host_add_device(struct host *this_host, char *new_device_name);
int host_add_sensor_to_device(struct host *this_host, char *device_name, char *new_sensor_name);

int host_add_engine(struct host *this_host, char *new_engine_name);
int host_add_device_to_engine(struct host *this_host, char *engine_name, char *new_device_name);
int host_add_sensor_to_engine_device(struct host *this_host, char *engine_name, char *device_name, char *new_sensor_name);

char *host_get_sensor_value(struct host *this_host, char *device_name, char *sensor_name);
char *host_get_sensor_status(struct host *this_host, char *device_name, char *sensor_name);
int host_update_sensor(struct host *this_host, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status);
int host_update_engine_sensor(struct host *this_host, char *engine_name, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status);

char *host_html_detail(struct host *this_host);
#endif

