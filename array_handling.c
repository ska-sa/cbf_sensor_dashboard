#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "array_handling.h"


struct cmc_array {
    char *name;
    int monitor_port;
};

char *read_full_katcp_line(struct katcl_line *l)
{
    int i = 0;
    char* line_to_return;
    line_to_return = malloc(1);
    memset(line_to_return, '\0', 1);
    //size_t current_line_length = 0;
    char* buffer;
    do {
        buffer = arg_copy_string_katcl(l, i++);
        if (buffer)
        {
            line_to_return = realloc(line_to_return, strlen(line_to_return) + strlen(buffer) + 2);
            line_to_return = strcat(line_to_return, buffer);
            line_to_return = strcat(line_to_return, " "); /* otherwise this becomes rather unreadable. */
        }
        free(buffer);
    } while (buffer);

    //line_to_return[strlen(line_to_return) - 2] = '\0'; /* remove the trailing space */

    return line_to_return;
}

struct cmc_array *create_array(char *array_name, int monitor_port)
{
    struct cmc_array *new_array = malloc(sizeof(*new_array));
    new_array->name = malloc(strlen(array_name) + 1);
    sprintf(new_array->name, "%s", array_name);
    new_array->monitor_port = monitor_port;
    return new_array;
}

char *get_array_name(struct cmc_array *array)
{
    char *array_name = malloc(strlen(array->name));
    sprintf(array_name, "%s", array->name);
    return array_name;
}

void destroy_array(struct cmc_array *array)
{
    free(array->name);
    free(array);
}


