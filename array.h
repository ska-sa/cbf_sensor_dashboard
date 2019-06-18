#ifndef _ARRAY_H_
#define _ARRAY_H_

struct array;

struct array *array_create(char *new_array_name, size_t number_of_antennas);
void array_destroy(struct array *this_array);

int array_add_team_host_device_sensor(struct array *this_array, char team_type, unsigned int host_number, char *device_name, char *sensor_name);
int array_add_team_host_engine_device_sensor(struct array *this_array, char team_type, unsigned int host_number, char *engine_name, char *device_name, char *sensor_name);

char *array_get_sensor_value(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name);
char *array_get_sensor_status(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name);
#endif
