#include <stdio.h>
#include <stdlib.h>

#include "host.h"
#include "tokenise.h"

#define BUF_SIZE 1024

int parse_sensor_list_file(char *filename)
{
    FILE *sensor_list_file = fopen(filename, "r");
    if (sensor_list_file == NULL)
        return -1;

    char buffer[BUF_SIZE];
    char *result;
    for (result = fgets(buffer, BUF_SIZE, sensor_list_file); result != NULL; result = fgets(buffer, BUF_SIZE, sensor_list_file))
    {
        printf("%s", buffer);
    } 
    return 0;
}


int main()
{

    char filename[] = "sensor_list";
    parse_sensor_list_file(filename);

    return 0;
}

