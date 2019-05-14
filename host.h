#ifdef _ARRAY_H_
#define _ARRAY_H_

#include "sensor.h"

struct host {
    char host_type;
    struct sensor **sensor_list;
    int number_of_sensors;
    int (*update_sensor_value)(struct *host, char*, char*);
};

struct host *create_host(char type);
void destroy_host(struct host *this_host);

#endif

