#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "host.h"
#include "tokenise.h"

#define BUF_SIZE 1024

int main()
{
    char filename[] = "sensor_list";
    int number_of_antennas = 4;
    int xengines_per_xhost = 4;
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

        switch (num_tokens) {
            case 3:
                switch (tokens[0][0]) {
                    case 'f':

                    case 'x':

                    default:
                        return -1;
                }
            case 4:

            default:
                return -1; /*for now*/
        }
    } 

    return 0;
}

