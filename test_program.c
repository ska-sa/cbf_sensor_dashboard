#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
        /*remove the newline character from the end of the string.*/
        buffer[strlen(buffer) - 1] = 0;

        char **tokens = NULL;
        int num_tokens;
        num_tokens = tokenise_string(buffer, '.', &tokens);
        if (num_tokens > 0)
        {   
            int i;
            for (i = 0; i < num_tokens; i++)
            {
                printf("%s ", tokens[i]);
            }
            printf("\n");
        }
    } 
    return 0;
}


int main()
{

    char filename[] = "sensor_list";
    parse_sensor_list_file(filename);

    return 0;
}

