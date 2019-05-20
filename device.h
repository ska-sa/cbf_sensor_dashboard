#ifndef _DEVICE_H_
#define _DEVICE_H_

struct device;

struct device *device_create(char *new_name, char **sensor_names, unsigned int number_of_sensors);
void device_destroy(struct device *this_device);
char *device_get_name(struct device *this_device);
void device_print_sensors(struct device *this_device);

#endif
