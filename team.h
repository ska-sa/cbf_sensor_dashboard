#ifndef _TEAM_H_
#define _TEAM_H_

struct team;

struct team *team_create(char type, unsigned int number_of_antennas);
void team_destroy(struct team *this_team);

int team_add_device_sensor(struct team *this_team, unsigned int host_number, char *device_name, char *sensor_name);
int team_add_engine_device_sensor(struct team *this_team, unsigned int host_number, char *engine_name, char *device_name, char *sensor_name);

#endif
