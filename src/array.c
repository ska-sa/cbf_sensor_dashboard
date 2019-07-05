#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <netc.h>
#include <katcp.h>
#include <katcl.h>

#include "array.h"
#include "team.h"
#include "message.h"
#include "verbose.h"
#include "queue.h"

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

enum array_state {
    ARRAY_SEND_FRONT_OF_QUEUE,
    ARRAY_WAIT_RESPONSE,
    ARRAY_MONITOR,
};


struct array {
    char *name;
    struct team **team_list;
    size_t number_of_teams;
    size_t number_of_antennas;
    uint16_t monitor_port;
    char *cmc_address;
    int monitor_fd;
    struct katcl_line *katcl_line;
    enum array_state state;
    struct queue *outgoing_msg_queue;
    struct message *current_message;
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
        new_array->number_of_teams = 2;
        new_array->team_list = malloc(sizeof(new_team->team_list)*(new_team->number_of_teams)); 
        new_array->team_list[0] = team_create('f', new_array->number_of_antennas);
        new_array->team_list[1] = team_create('x', new_array->number_of_antennas);
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


void array_set_fds(struct array *this_array, fd_set *rd, fd_set *wr, int *nfds)
{
    FD_SET(this_array->monitor_fd, rd);
    if (flushing_katcl(this_array->katcl_line))
    {
        FD_SET(this_array->monitor_fd, wr);
    }
    *nfds = max(*nfds, this_array->monitor_fd);
}


void array_setup_katcp_writes(struct array *this_array)
{
    if (this_array->current_message)
    {
        if (this_array->state == ARRAY_SEND_FRONT_OF_QUEUE)
        {
            int n = message_get_number_of_words(this_array->current_message);
            if (n > 0)
            {
                char *composed_message = message_compose(this_array->current_message);
                verbose_message(BORING, "Sending KATCP message \"%s\" to %s:%hu\n", composed_message, this_array->cmc_address, this_array->monitor_port);
                free(composed_message);
                composed_message = NULL;

                char *first_word = malloc(strlen(message_see_word(this_array->current_message, 0)) + 2);
                sprintf(first_word, "%c%s", message_get_type(this_array->current_message), message_see_word(this_array->current_message, 0));
                if (message_get_number_of_words(this_array->current_message) == 1)
                    append_string_katcl(this_array->katcl_line, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, first_word);
                else
                {
                    append_string_katcl(this_array->katcl_line, KATCP_FLAG_FIRST, first_word);
                    size_t j;
                    for (j = 1; j < n - 1; j++)
                    {
                        append_string_katcl(this_array->katcl_line, 0, message_see_word(this_array->current_message, j));
                    }
                    append_string_katcl(this_array->katcl_line, KATCP_FLAG_LAST, message_see_word(this_array->current_message, (size_t) n - 1));
                }
                free(first_word);
                first_word = NULL;
            }
            else
            {
                verbose_message(WARNING, "A message on %s:%hu's queue had 0 words in it. Cannot send.\n", this_array->cmc_address, this_array->monitor_port);
            }
            this_array->state = ARRAY_WAIT_RESPONSE;
        }
    }
}


void array_socket_read_write(struct array *this_array, fd_set *rd, fd_set *wr)
{
    int r;
    if (FD_ISSET(this_array->monitor_fd, rd))
    {
        verbose_message(BORING, "Reading katcl_line from %s:%hu.\n", this_array->cmc_address, this_array->monitor_port);
        r = read_katcl(this_array->katcl_line);
        if (r)
        {
            fprintf(stderr, "read from %s:%hu failed\n", this_array->cmc_address, this_array->monitor_port);
            perror("read_katcl()");
            /*TODO some kind of error checking, what to do if the CMC doesn't connect.*/
        }
    }

    if (FD_ISSET(this_array->monitor_fd, wr))
    {
        verbose_message(BORING, "Writing katcl_line to %s:%hu.\n", this_array->cmc_address, this_array->monitor_port);
        r = write_katcl(this_array->katcl_line);
        if (r < 0)
        {
            perror("write_katcl");
            /*TODO some other kind of error checking.*/
        }
    }
}


void array_handle_received_katcl_lines(struct array *this_array)
{
    while (have_katcl(this_array->katcl_line) > 0)
    {
        verbose_message(BORING, "From %s:%hu: %s %s %s %s %s\n", this_array->cmc_address, this_array->monitor_port, \
                arg_string_katcl(this_array->katcl_line, 0), \
                arg_string_katcl(this_array->katcl_line, 1), \
                arg_string_katcl(this_array->katcl_line, 2), \
                arg_string_katcl(this_array->katcl_line, 3), \
                arg_string_katcl(this_array->katcl_line, 4)); 
        char received_message_type = arg_string_katcl(this_array->katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_array->katcl_line, 0) + 1, message_see_word(this_array->current_message, 0)))
                {
                    if (!strcmp(arg_string_katcl(this_array->katcl_line, 1), "ok"))
                    {
                        verbose_message(DEBUG, "%s:%hu received %s ok!\n", this_array->cmc_address, this_array->monitor_port, message_see_word(this_array->current_message, 0));
                        this_array->state = ARRAY_SEND_FRONT_OF_QUEUE;
                        verbose_message(BORING, "%s:%hu still has %u message(s) in its queue...\n", this_array->cmc_address, this_array->monitor_port, queue_sizeof(this_array->outgoing_msg_queue));
                        if (queue_sizeof(this_array->outgoing_msg_queue))
                        {
                            verbose_message(BORING, "%s:%hu  popping queue...\n", this_array->cmc_address, this_array->monitor_port);
                            array_queue_pop(this_array);
                        }
                        else
                        {
                            verbose_message(INFO, "%s:%hu going into monitoring state.\n", this_array->cmc_address, this_array->monitor_port);
                            message_destroy(this_array->current_message);
                            this_array->current_message = NULL; //doesn't do this in the above function. C problem.
                            this_array->state = ARRAY_MONITOR;
                        }
                    }
                    else 
                    {
                        verbose_message(WARNING, "Received %s %s. Retrying the request...", message_see_word(this_array->current_message, 0), arg_string_katcl(this_array->katcl_line, 1));
                        this_array->state = ARRAY_SEND_FRONT_OF_QUEUE;
                    }

                }
                break;
            case '#': // it's a katcp inform
                if (!strcmp(arg_string_katcl(this_array->katcl_line, 0) + 1, "array-list"))
                {

                }
                else if (!strcmp(arg_string_katcl(this_array->katcl_line, 0) + 1, "group-created"))
                {

                }
                else if (!strcmp(arg_string_katcl(this_array->katcl_line, 0) + 1, "group-destroyed"))
                {
                    
                }
                break;
            default:
                verbose_message(WARNING, "Unexpected KATCP message received, starting with %c\n", received_message_type);
        }
    }
}


char *array_html_summary(struct array *this_array, char *cmc_name)
{
    char *format = "<tr><td><a href=\"%s/%s\">%s</a></td><td>%hu</td><td>%lu</td>";
    ssize_t needed = snprintf(NULL, 0, format, cmc_name, this_array->name, this_array->name, this_array->monitor_port, this_array->number_of_antennas) + 1;
    //TODO checks
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, cmc_name, this_array->name, this_array->name, this_array->monitor_port, this_array->number_of_antennas);
    return html_summary;
}


char *array_html_detail(struct array *this_array)
{
    //TODO this is where the fun stuff is going to be.
    return NULL;
}


struct message *array_queue_pop(struct array *this_array)
{
    if (this_array->current_message != NULL)
    {
        message_destroy(this_array->current_message);
    }
    this_array->current_message = queue_pop(this_array->outgoing_msg_queue);
    return this_array->current_message;
}
