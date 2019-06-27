#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <netc.h>

#include "array.h"
#include "team.h"

struct array {
   char *name;
   struct team **team_list;
   size_t number_of_teams;
   size_t number_of_antennas;
   uint16_t monitor_port;
   char *cmc_address;
   int monitor_fd;
};


struct array *array_create(char *new_array_name, char *cmc_address, uint16_t monitor_port, size_t number_of_antennas)
{
   struct array *new_array = malloc(sizeof(*new_array));
   if (new_array != NULL)
   {
        new_array->name = strdup(new_array_name);
        new_array->number_of_antennas = number_of_antennas;
        new_array->cmc_address = strdup(cmc_address);
        new_array->monitor_port = monitor_port;
        new_array->team_list = NULL; /*Make it explicit, will fill this later from a config file.*/
   }
   return new_array;
}


void array_destroy(struct array *this_array)
{
    if (this_array != NULL)
    {
        free(this_array->cmc_address);
        free(this_array->name);
        size_t i;
        for (i = 0; i < this_array->number_of_teams; i++)
        {
            team_destroy(this_array->team_list[i]);
        }
        free(this_array);
        this_array = NULL;
    }
}


char *array_get_name(struct array *this_array)
{
    return this_array->name;
}


int array_add_team_host_device_sensor(struct array *this_array, char team_type, unsigned int host_number, char *device_name, char *sensor_name)
{
    if (this_array != NULL)
    {
       size_t i;
       for (i = 0; i < this_array->number_of_teams; i++)
       {
          if (team_get_type(this_array->team_list[i]) == team_type)
          {
              return team_add_device_sensor(this_array->team_list[i], host_number, device_name, sensor_name);
          }
       }
       /*if we've gotten to this point, the team doesn't exist yet.*/
       struct team **temp = realloc(this_array->team_list, sizeof(*(this_array->team_list))*(this_array->number_of_teams + 1));
       if (temp != NULL)
       {
           this_array->team_list = temp;
           this_array->team_list[this_array->number_of_teams] = team_create(team_type, this_array->number_of_antennas);
           this_array->number_of_teams++;
           return team_add_device_sensor(this_array->team_list[i], host_number, device_name, sensor_name);
       }
    }
    return -1;
}


int array_add_team_host_engine_device_sensor(struct array *this_array, char team_type, unsigned int host_number, char *engine_name, char *device_name, char *sensor_name)
{
    if (this_array != NULL)
    {
        size_t i;
        for (i = 0; i < this_array->number_of_teams; i++)
        {
            if (team_get_type(this_array->team_list[i]) == team_type)
            {
                return team_add_engine_device_sensor(this_array->team_list[i], host_number, engine_name, device_name, sensor_name);
            }
        }
        /*if we've gotten to this point, the team doesn't exist yet.*/
        struct team **temp = realloc(this_array->team_list, sizeof(*(this_array->team_list))*(this_array->number_of_teams + 1));
        if (temp != NULL)
        {
            this_array->team_list = temp;
            this_array->team_list[this_array->number_of_teams] = team_create(team_type, this_array->number_of_antennas);
            this_array->number_of_teams++;
            return team_add_device_sensor(this_array->team_list[i], host_number, device_name, sensor_name);
        }
    }
    return -1;
}


char *array_get_sensor_value(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name)
{
    if (this_array != NULL)
    {
        size_t i;
        for (i = 0; i < this_array->number_of_teams; i++)
        {
            if (team_type == team_get_type(this_array->team_list[i]))
                return team_get_sensor_value(this_array->team_list[i], host_number, device_name, sensor_name);
        }
    }
    return NULL;
}


char *array_get_sensor_status(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name)
{
    if (this_array != NULL)
    {
        size_t i;
        for (i = 0; i < this_array->number_of_teams; i++)
        {
            if (team_type == team_get_type(this_array->team_list[i]))
                return team_get_sensor_status(this_array->team_list[i], host_number, device_name, sensor_name);
        }
    }
    return NULL;

}
