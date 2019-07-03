#ifndef _ARRAY_H_
#define _ARRAY_H_
#include <stdint.h>

struct array;

struct array *array_create(char *new_array_name, char *cmc_address, uint16_t monitor_port, size_t number_of_antennas);
void array_destroy(struct array *this_array);

char *array_get_name(struct array *this_array);
int array_add_team_host_device_sensor(struct array *this_array, char team_type, unsigned int host_number, char *device_name, char *sensor_name);
int array_add_team_host_engine_device_sensor(struct array *this_array, char team_type, unsigned int host_number, char *engine_name, char *device_name, char *sensor_name);

char *array_get_sensor_value(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name);
char *array_get_sensor_status(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name);

char *array_html_summary(struct array *this_array, char *cmc_name);

#endif
