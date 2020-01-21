#ifndef _ENGINE_H_
#define _ENGINE_H_

#include <time.h>

/**
 * \file  engine.h
 * \brief The engine type stores devices when there are multiple engines on a host.
 *        It is a member of a device object, but is also watched by a vdevice object,
 *        which accesses the underlying devices to report the compound status of the devices.
 */

struct engine;

struct engine *engine_create(char *new_name);
void engine_destroy(struct engine *this_engine);
char *engine_get_name(struct engine *this_engine);
int engine_add_device(struct engine *this_engine, char *new_device_name);
int engine_add_sensor_to_device(struct engine *this_engine, char *device_name, char *new_sensor_name);
char *engine_get_sensor_value(struct engine *this_engine, char *device_name, char *sensor_name);
char *engine_get_sensor_status(struct engine *this_engine, char *device_name, char *sensor_name);
int engine_update_sensor(struct engine *this_engine, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status);

/*debug functions*/
//void engine_print(struct engine *this_engine);

#endif

