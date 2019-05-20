#ifdef _ARRAY_H_
#define _ARRAY_H_

#include "sensor.h"

struct host {
    char *host_name;
    char host_type;
    struct sensor **sensor_list;
    int number_of_sensors;
    int (*update_sensor_value)(struct *host, char*, char*);
};

struct host *host_create(char type);
void host_destroy(struct host *this_host);

#endif

