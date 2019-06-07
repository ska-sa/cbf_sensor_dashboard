#include <stdlib.h>
#include <string.h>

#include "team.h"
#include "host.h"


struct team {
    char host_type;
    unsigned int number_of_antennas;
    struct host **host_list;
};


struct team *team_create(char type, unsigned int number_of_antennas)
{
    struct team *new_team = malloc(sizeof(*new_team));
    if (new_team != NULL)
    {
        new_team->host_type = type;
        new_team->number_of_antennas = number_of_antennas;
        new_team->host_list = malloc(sizeof(new_team->host_list)*new_team->number_of_antennas);
        int i;
        for (i = 0; i < new_team->number_of_antennas; i++)
        {
            new_team->host_list[i] = host_create();
        }
    }
    return new_team;
}


void team_destroy(struct team *this_team)
{
    if (this_team != NULL)
    {
        unsigned int i;
        for (i = 0; i < this_team->number_of_antennas; i++)
        {
            host_destroy(this_team->host_list[i]);
        }
        free(this_team->host_list);
        free(this_team);
        this_team = NULL;
    }
}


int team_add_device_sensor(struct team *this_team, unsigned int host_number, char *device_name, char *sensor_name)
{
    if (this_team != NULL && host_number < this_team->number_of_antennas)
    {
        int r;
        r = host_add_device(this_team->host_list[host_number], device_name);
        if (r < 0)
            return r;
        r = host_add_sensor_to_device(this_team->host_list[host_number], device_name, sensor_name);
        if (r < 0)
            return r;
        return 0;
    }
    return -1;
}


int team_add_engine_device_sensor(struct team *this_team, unsigned int host_number, char *engine_name, char *device_name, char *sensor_name)
{
    if (this_team != NULL && host_number < this_team->number_of_antennas)
    {
        int r;
        r = host_add_engine(this_team->host_list[host_number], engine_name);
        if (r < 0)
            return r;
        r = host_add_device_to_engine(this_team->host_list[host_number], engine_name, device_name);
        if (r < 0)
            return r;
        r = host_add_sensor_to_engine_device(this_team->host_list[host_number], engine_name, device_name, sensor_name);
        if (r < 0)
            return r;
        return 0;
    }
    return -1;
}
