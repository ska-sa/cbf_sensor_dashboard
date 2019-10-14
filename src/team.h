#ifndef _TEAM_H_
#define _TEAM_H_

struct team;

struct team *team_create(char type, size_t number_of_antennas);
void team_destroy(struct team *this_team);

int team_add_device_sensor(struct team *this_team, size_t host_number, char *device_name, char *sensor_name);
int team_add_engine_device_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char *sensor_name);

int team_set_host_serial_no(struct team *this_team, size_t host_number, char *host_serial);

char team_get_type(struct team *this_team);

int team_update_sensor(struct team *this_team, size_t host_number, char *device_name, char*sensor_name, char *new_sensor_value, char *new_sensor_status);
int team_update_engine_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status);

char *team_get_sensor_value(struct team *this_team, size_t host_number, char *device_name, char *sensor_name);
char *team_get_sensor_status(struct team *this_team, size_t host_number, char *device_name, char *sensor_name);

char *team_get_host_html_detail(struct team *this_team, size_t host_number);
#endif
