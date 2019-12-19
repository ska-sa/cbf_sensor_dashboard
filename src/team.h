#ifndef _TEAM_H_
#define _TEAM_H_

/**
 * \file  team.h
 * \brief The team class is for organising hosts in an array. It has no direct analogue in the actual correlator.
 *        It's just easier to manage F- and X-hosts in groups separately.
 */

struct team;

struct team *team_create(char type, size_t number_of_antennas);
void team_destroy(struct team *this_team);

int team_add_device_sensor(struct team *this_team, size_t host_number, char *device_name, char *sensor_name);
int team_add_engine_device_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char *sensor_name);

int team_set_host_serial_no(struct team *this_team, size_t host_number, char *host_serial);

char team_get_type(struct team *this_team);

int team_set_fhost_input_stream(struct team *this_team, char *input_stream_name, size_t fhost_number);
char *team_get_fhost_input_stream(struct team *this_team, size_t fhost_number);
int team_update_sensor(struct team *this_team, size_t host_number, char *device_name, char*sensor_name, char *new_sensor_value, char *new_sensor_status);
int team_update_engine_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status);

char *team_get_sensor_value(struct team *this_team, size_t host_number, char *device_name, char *sensor_name);
char *team_get_sensor_status(struct team *this_team, size_t host_number, char *device_name, char *sensor_name);

char** team_get_stagnant_sensor_names(struct team *this_team, time_t stagnant_time, size_t *number_of_sensors);

char *team_get_host_html_detail(struct team *this_team, size_t host_number);
#endif
