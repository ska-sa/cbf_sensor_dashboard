#include <stdlib.h>
#include <string.h>

#include "array.h"
#include "team.h"

struct array {
   char *name;
   struct team **team_list;
   size_t number_of_teams;
   size_t number_of_antennas;
};


struct array *array_create(char *new_array_name, size_t number_of_antennas)
{
   struct array *new_array = malloc(sizeof(*new_array));
   if (new_array != NULL)
   {
        new_array->name = strdup(new_array_name);
        new_array->number_of_antennas = number_of_antennas;
        new_array->team_list = NULL; /*Make it explicit, will fill this later from a config file.*/
   }
   return new_array;
}


