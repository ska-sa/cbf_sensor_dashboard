#ifndef _ARRAY_H_
#define _ARRAY_H_
#include <stdint.h>
#include "message.h"

/**
 * \file  array.h
 * \brief The array class stores a list of teams (which in turn store hosts) to manage the data representing the state of the correlator.
 *        The class also has members for managing the connection and communication with the corr2_servlet and corr2_sensor_servlet of each array.
 */

struct array;

struct array *array_create(char *new_array_name, char *cmc_address, uint16_t control_port, uint16_t monitor_port, size_t n_antennas);
void array_destroy(struct array *this_array);

char *array_get_name(struct array *this_array);
size_t array_get_size(struct array *this_array);
int array_add_team_host_device_sensor(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name);
int array_add_team_host_engine_device_sensor(struct array *this_array, char team_type, size_t host_number, char *engine_name, char *device_name, char *sensor_name);
int array_add_top_level_sensor(struct array *this_array, char *sensor_name);
int array_update_top_level_sensor(struct array *this_array, char *sensor_name, char *new_value, char *new_status);

void array_mark_suspect(struct array *this_array);
int array_check_suspect(struct array *this_array);
void array_mark_fine(struct array *this_array);

int array_functional(struct array *this_array);

char *array_get_sensor_value(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name);
char *array_get_sensor_status(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name);

void array_set_fds(struct array *this_array, fd_set *rd, fd_set *wr, int *nfds);
void array_setup_katcp_writes(struct array *this_array);
void array_socket_read_write(struct array *this_array, fd_set *rd, fd_set *wr);
void array_handle_received_katcl_lines(struct array *this_array);

struct message *array_control_queue_pop(struct array *this_array);
struct message *array_monitor_queue_pop(struct array *this_array);

char *array_html_summary(struct array *this_array, char *cmc_name);
char *array_html_detail(struct array *this_array);
char *array_html_missing_pkt_view(struct array *this_array);

//char** array_get_stagnant_sensor_names(struct array *this_array, time_t stagnant_time, size_t *number_of_sensors);

#endif
