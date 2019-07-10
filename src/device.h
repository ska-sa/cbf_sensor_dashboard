#ifndef _DEVICE_H_
#define _DEVICE_H_

struct device;

struct device *device_create(char *new_name);
void device_destroy(struct device *this_device);
char *device_get_name(struct device *this_device);
int device_add_sensor(struct device *this_device, char *new_sensor_name); /* I don't think we need the capability to remove sensors for the time being. */
char **device_get_sensor_names(struct device *this_device, unsigned int *number_of_sensors);
char *device_get_sensor_value(struct device *this_device, char *sensor_name);
char *device_get_sensor_status(struct device *this_device, char *sensor_name);
int device_update_sensor(struct device *this_device, char *sensor_name, char *new_sensor_value, char *new_sensor_status);

char *device_html_summary(struct device *this_device);

/* functions for debugging, for removal later. */
void device_print_sensors(struct device *this_device);

#endif
