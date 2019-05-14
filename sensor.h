#ifndef _SENSOR_H_
#define _SENSOR_H_

struct sensor {
    char *name;
    char *value;
    int (*update_value)(struct sensor*, char*);
};

struct sensor *create_sensor(char *name);
void destroy_sensor(struct sensor *the_sensor);

#endif

