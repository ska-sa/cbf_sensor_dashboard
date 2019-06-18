#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "team.h"
#include "tokenise.h"
#include "queue.h"

#define BUF_SIZE 1024

int main()
{
    char filename[] = "sensor_list";
    int number_of_antennas = 64;
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
        size_t num_tokens;
        num_tokens = tokenise_string(buffer, '.', &tokens);
        size_t i, j;

        switch (tokens[0][0]) {
            case 'f':
                /*TODO create some fhosts.*/
                for (i = 0; i < number_of_antennas; i++)
                    printf("?sensor-sampling %s%02ld.%s.%s auto\n", tokens[0], i, tokens[1], tokens[2]);
                break;
            case 'x':
                switch (num_tokens) {
                    case 3:
                        ; /* empty statement so that I can declare a char straight afterwards */
                        /*TODO create some fhosts.*/
                        char *amark = strchr(tokens[2], '%');
                        for (i = 0; i < number_of_antennas; i++)
                        {
                            /*Check whether there's the % character in the sensor name. With corr2_sensor_servlet at the moment, xhost.missing-pkts.fhost%%-cnt is the only place this occurs.*/
                            if (amark != NULL)
                            {
                                amark[0] = '\0';
                                size_t k;
                                for (k = 0; k < number_of_antennas; k++)
                                {
                                    printf("?sensor-sampling %s%02ld.%s.%s%02ld%s auto\n", tokens[0], i, tokens[1], tokens[2], k, amark + 1);
                                }
                            }
                            else
                            {
                                printf("?sensor-sampling %s%02ld.%s.%s auto\n", tokens[0], i, tokens[1], tokens[2]);
                            }
                        }
                        /*don't need to free it because the memory is allocated to another pointer,
                         * but it's best not to leave the pointer pointing there. */
                        amark = NULL;
                        break;
                    case 4:
                        /*TODO create some xhosts.*/
                        for (i = 0; i < number_of_antennas; i++)
                            for (j = 0; j < xengines_per_xhost; j++)
                                printf("?sensor-sampling %s%02ld.%s%02ld.%s.%s auto\n", tokens[0], i, tokens[1], j, tokens[2], tokens[3]);
                        break;
                    default:
                        printf("got a number of tokens other than 3 or 4: %s\n", buffer);
                }
                break;
            default:
                printf("got a host kind other than x or f: {%c} from %s\n", tokens[0][0], buffer);
                printf("Tokens: ");
                for (i = 0; i < num_tokens; i++)
                    printf("%s ", tokens[i]);
                printf("\n");
        }

        for (i = 0; i < num_tokens; i++)
        {
            free(tokens[i]);
        }
        free(tokens);
        tokens = NULL; 
    } 
    fclose(sensor_list_file);

    return 0;
}

