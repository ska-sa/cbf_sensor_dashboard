#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <netc.h>
#include <katcp.h>
#include <katcl.h>

#include "array.h"
#include "team.h"
#include "message.h"
#include "verbose.h"
#include "queue.h"
#include "tokenise.h"

#define BUF_SIZE 1024
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

enum array_state {
    ARRAY_SEND_FRONT_OF_QUEUE,
    ARRAY_WAIT_RESPONSE,
    ARRAY_MONITOR,
    ARRAY_DISCONNECTED = -1, //this must be the last thing in the enum so that it gives an error in another place.
};


struct array {
    char *name;
    int array_is_active;

    struct team **team_list;
    size_t number_of_teams;
    size_t n_antennas;

    char *cmc_address;

    time_t last_updated;

    uint16_t control_port;
    int control_fd;
    struct katcl_line *control_katcl_line;
    enum array_state control_state;
    struct queue *outgoing_control_msg_queue;
    struct message *current_control_message;

    char *instrument_state;
    char *config_file;

    uint16_t monitor_port;
    int monitor_fd;
    struct katcl_line *monitor_katcl_line;
    enum array_state monitor_state;
    struct queue *outgoing_monitor_msg_queue;
    struct message *current_monitor_message;
};


struct array *array_create(char *new_array_name, char *cmc_address, uint16_t control_port, uint16_t monitor_port, size_t n_antennas)
{
   struct array *new_array = malloc(sizeof(*new_array));
   if (new_array != NULL)
   {
        new_array->name = strdup(new_array_name);
        new_array->array_is_active = 1;
        new_array->n_antennas = n_antennas;
        new_array->cmc_address = strdup(cmc_address);

        new_array->last_updated = time(0);

        new_array->control_port = control_port;
        new_array->control_fd = net_connect(cmc_address, control_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
        new_array->control_katcl_line = create_katcl(new_array->control_fd);
        new_array->outgoing_control_msg_queue = queue_create();

        struct message *new_message = message_create('?');
        message_add_word(new_message, "log-local");
        message_add_word(new_message, "off");
        queue_push(new_array->outgoing_control_msg_queue, new_message);

        new_message = message_create('?');
        message_add_word(new_message, "sensor-sampling");
        message_add_word(new_message, "instrument-state");
        message_add_word(new_message, "auto");
        queue_push(new_array->outgoing_control_msg_queue, new_message);
        new_array->instrument_state = strdup("-");
        new_array->config_file = strdup("-");
        new_array->current_control_message = NULL;
        array_control_queue_pop(new_array);
        new_array->control_state = ARRAY_SEND_FRONT_OF_QUEUE;

        new_array->monitor_port = monitor_port;
        new_array->monitor_fd = net_connect(cmc_address, monitor_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
        new_array->monitor_katcl_line = create_katcl(new_array->monitor_fd);
        new_array->outgoing_monitor_msg_queue = queue_create();

        new_message = message_create('?');
        message_add_word(new_message, "log-local");
        message_add_word(new_message, "off");
        queue_push(new_array->outgoing_monitor_msg_queue, new_message);
        new_array->current_monitor_message = NULL;
        array_monitor_queue_pop(new_array);
        new_array->monitor_state = ARRAY_SEND_FRONT_OF_QUEUE;

        new_array->number_of_teams = 2;
        new_array->team_list = malloc(sizeof(new_array->team_list)*(new_array->number_of_teams));
        new_array->team_list[0] = team_create('f', new_array->n_antennas);
        new_array->team_list[1] = team_create('x', new_array->n_antennas);
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
        free(this_array->team_list);

        destroy_katcl(this_array->control_katcl_line, 1);
        close(this_array->control_fd);
        queue_destroy(this_array->outgoing_control_msg_queue);
        message_destroy(this_array->current_control_message);

        destroy_katcl(this_array->monitor_katcl_line, 1);
        close(this_array->monitor_fd);
        queue_destroy(this_array->outgoing_monitor_msg_queue);
        message_destroy(this_array->current_monitor_message);

        free(this_array->instrument_state);
        free(this_array->config_file);

        free(this_array);
        this_array = NULL;
    }
}


char *array_get_name(struct array *this_array)
{
    return this_array->name;
}


int array_add_team_host_device_sensor(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name)
{
    verbose_message(BORING, "Adding sensor: %chost%02d.%s.%s\n", team_type, host_number, device_name, sensor_name);
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
           this_array->team_list[this_array->number_of_teams] = team_create(team_type, this_array->n_antennas);
           this_array->number_of_teams++;
           return team_add_device_sensor(this_array->team_list[i], host_number, device_name, sensor_name);
       }
    }
    return -1;
}


int array_add_team_host_engine_device_sensor(struct array *this_array, char team_type, size_t host_number, char *engine_name, char *device_name, char *sensor_name)
{
    verbose_message(BORING, "Adding sensor: %chost%02d.%s.%s.%s\n", team_type, host_number, engine_name, device_name, sensor_name);
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
            this_array->team_list[this_array->number_of_teams] = team_create(team_type, this_array->n_antennas);
            this_array->number_of_teams++;
            return team_add_device_sensor(this_array->team_list[i], host_number, device_name, sensor_name);
        }
    }
    return -1;
}


void array_mark_suspect(struct array *this_array)
{
    this_array->array_is_active = 0;
}


int array_check_suspect(struct array *this_array)
{
    if (this_array != NULL)
        return !this_array->array_is_active; //i.e. it's not suspect if it's active.
    else
        return -1;
}


void array_mark_fine(struct array *this_array)
{
    this_array->array_is_active = 1;
}


int array_functional(struct array *this_array)
{
    if (this_array->control_state == ARRAY_DISCONNECTED || this_array->monitor_state == ARRAY_DISCONNECTED)
        return -1;
    else
        return 1;
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
    FD_SET(this_array->control_fd, rd);
    if (flushing_katcl(this_array->control_katcl_line))
    {
        FD_SET(this_array->control_fd, wr);
    }
    *nfds = max(*nfds, this_array->control_fd);

    FD_SET(this_array->monitor_fd, rd);
    if (flushing_katcl(this_array->monitor_katcl_line))
    {
        FD_SET(this_array->monitor_fd, wr);
    }
    *nfds = max(*nfds, this_array->monitor_fd);
}


void array_setup_katcp_writes(struct array *this_array)
{
    if (this_array->current_control_message)
    {
        if (this_array->control_state == ARRAY_SEND_FRONT_OF_QUEUE)
        {
            int n = message_get_number_of_words(this_array->current_control_message);
            if (n > 0)
            {
                char *composed_message = message_compose(this_array->current_control_message);
                verbose_message(BORING, "Sending KATCP message \"%s\" to %s:%hu\n", composed_message, this_array->cmc_address, this_array->control_port);
                free(composed_message);
                composed_message = NULL;

                char *first_word = malloc(strlen(message_see_word(this_array->current_control_message, 0)) + 2);
                sprintf(first_word, "%c%s", message_get_type(this_array->current_control_message), message_see_word(this_array->current_control_message, 0));
                if (message_get_number_of_words(this_array->current_control_message) == 1)
                    append_string_katcl(this_array->control_katcl_line, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, first_word);
                else
                {
                    append_string_katcl(this_array->control_katcl_line, KATCP_FLAG_FIRST, first_word);
                    size_t j;
                    for (j = 1; j < n - 1; j++)
                    {
                        append_string_katcl(this_array->control_katcl_line, 0, message_see_word(this_array->current_control_message, j));
                    }
                    append_string_katcl(this_array->control_katcl_line, KATCP_FLAG_LAST, message_see_word(this_array->current_control_message, (size_t) n - 1));
                }
                free(first_word);
                first_word = NULL;

                this_array->control_state = ARRAY_WAIT_RESPONSE;
            }
            else
            {
                verbose_message(WARNING, "A message on %s:%hu's (control) queue had 0 words in it. Cannot send.\n", this_array->cmc_address, this_array->control_port);
                //TODO push to the next message.
            }
        }
    }

    if (this_array->current_monitor_message)
    {
        if (this_array->monitor_state == ARRAY_SEND_FRONT_OF_QUEUE)
        {
            int n = message_get_number_of_words(this_array->current_monitor_message);
            if (n > 0)
            {
                char *composed_message = message_compose(this_array->current_monitor_message);
                verbose_message(INFO, "Sending KATCP message \"%s\" to %s:%hu\n", composed_message, this_array->cmc_address, this_array->monitor_port);
                free(composed_message);
                composed_message = NULL;

                char *first_word = malloc(strlen(message_see_word(this_array->current_monitor_message, 0)) + 2);
                sprintf(first_word, "%c%s", message_get_type(this_array->current_monitor_message), message_see_word(this_array->current_monitor_message, 0));
                if (message_get_number_of_words(this_array->current_monitor_message) == 1)
                    append_string_katcl(this_array->monitor_katcl_line, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, first_word);
                else
                {
                    append_string_katcl(this_array->monitor_katcl_line, KATCP_FLAG_FIRST, first_word);
                    size_t j;
                    for (j = 1; j < n - 1; j++)
                    {
                        append_string_katcl(this_array->monitor_katcl_line, 0, message_see_word(this_array->current_monitor_message, j));
                    }
                    append_string_katcl(this_array->monitor_katcl_line, KATCP_FLAG_LAST, message_see_word(this_array->current_monitor_message, (size_t) n - 1));
                }
                free(first_word);
                first_word = NULL;

                this_array->monitor_state = ARRAY_WAIT_RESPONSE;
            }
            else
            {
                verbose_message(WARNING, "A message on %s:%hu's (monitor) queue had 0 words in it. Cannot send.\n", this_array->cmc_address, this_array->monitor_port);
                //TODO push to the next message.
            }
        }
    }
}


void array_socket_read_write(struct array *this_array, fd_set *rd, fd_set *wr)
{
    int r;
    if (FD_ISSET(this_array->control_fd, rd))
    {
        verbose_message(BORING, "Reading katcl_line from %s:%hu (control).\n", this_array->cmc_address, this_array->control_port);
        r = read_katcl(this_array->control_katcl_line);
        if (r)
        {
            verbose_message(ERROR, "read from %s:%hu (control) failed\n", this_array->cmc_address, this_array->control_port);
            this_array->control_state = ARRAY_DISCONNECTED;
        }
    }

    if (FD_ISSET(this_array->control_fd, wr))
    {
        verbose_message(BORING, "Writing katcl_line to %s:%hu (control).\n", this_array->cmc_address, this_array->control_port);
        r = write_katcl(this_array->control_katcl_line);
        if (r < 0)
        {
            verbose_message(ERROR, "write to from %s:%hu (control) failed\n", this_array->cmc_address, this_array->control_port);
            this_array->control_state = ARRAY_DISCONNECTED;
        }
    }

    if (FD_ISSET(this_array->monitor_fd, rd))
    {
        verbose_message(BORING, "Reading katcl_line from %s:%hu (monitor).\n", this_array->cmc_address, this_array->monitor_port);
        r = read_katcl(this_array->monitor_katcl_line);
        if (r)
        {
            verbose_message(ERROR, "read from %s:%hu (monitor) failed\n", this_array->cmc_address, this_array->monitor_port);
            this_array->monitor_state = ARRAY_DISCONNECTED;
        }
    }

    if (FD_ISSET(this_array->monitor_fd, wr))
    {
        verbose_message(BORING, "Writing katcl_line to %s:%hu (monitor).\n", this_array->cmc_address, this_array->monitor_port);
        r = write_katcl(this_array->monitor_katcl_line);
        if (r < 0)
        {
            verbose_message(ERROR, "write to from %s:%hu (monitor) failed\n", this_array->cmc_address, this_array->monitor_port);
            this_array->monitor_state = ARRAY_DISCONNECTED;
        }
    }
}


static void array_activate(struct array *this_array)
{
    verbose_message(INFO, "Detected %s (monitor port %s:%hu) in nominal state, subscribing to sensors.\n", this_array->name, this_array->cmc_address, this_array->monitor_port);
    FILE *config_file = fopen("conf/sensor_list.conf", "r");

    char buffer[BUF_SIZE];
    char *result;

    for (result = fgets(buffer, BUF_SIZE, config_file); result != NULL; result = fgets(buffer, BUF_SIZE, config_file))
    {
        verbose_message(DEBUG, "Read line from sensor-list file: %s", buffer);
        char **tokens = NULL;
        size_t n_tokens = tokenise_string(buffer, '.', &tokens);
        if (!((n_tokens == 2) || (n_tokens == 3))) //can't think of a better way to put it than this. Might be just bare device,
                                                 //or might be engine and device.
        {
            verbose_message(ERROR, "sensor_list.conf has a malformed sensor name: %s.\n", buffer);
        }
        else
        {
            char team_type = tokens[0][0]; //should be just "f" or "x" at this point.
            size_t i;
            if (n_tokens == 2)
            {
                for (i = 0; i < this_array->n_antennas; i++)
                {
                    array_add_team_host_device_sensor(this_array, team_type, i, tokens[1], "device-status");

                    char format[] = "%chost%02lu.%s.device-status"; //FOOBAR
                    ssize_t needed = snprintf(NULL, 0, format, team_type, i, tokens[1]) + 1;
                    char *sensor_string = malloc((size_t) needed); //TODO check for errors.
                    sprintf(sensor_string, format, team_type, i, tokens[1]);

                    struct message *new_message = message_create('?');
                    message_add_word(new_message, "sensor-sampling");
                    message_add_word(new_message, sensor_string);
                    message_add_word(new_message, "auto");
                    free(sensor_string);
                    queue_push(this_array->outgoing_monitor_msg_queue, new_message);
                }
            }
            if (n_tokens == 3)
            {
                for (i = 0; i < this_array->n_antennas; i++)
                {
                    size_t j;
                    for (j = 0; j < 4; j++)
                    {
                        //TODO formulate engine name.
                        char *engine_name;
                        char format[] = "%s%u";
                        ssize_t needed = snprintf(NULL, 0, format, tokens[1], j) + 1;
                        engine_name = malloc((size_t) needed);
                        sprintf(engine_name, format, tokens[1], j);
                        array_add_team_host_engine_device_sensor(this_array, team_type, i, engine_name, tokens[2], "device-status");

                        {
                            char format[] = "%chost%02lu.%s.%s.device-status";
                            ssize_t needed = snprintf(NULL, 0, format, team_type, i, engine_name, tokens[2], "device-status") + 1;
                            char *sensor_string = malloc((size_t) needed); //TODO check for errors.
                            sprintf(sensor_string, format, team_type, i, engine_name, tokens[2]);

                            struct message *new_message = message_create('?');
                            message_add_word(new_message, "sensor-sampling");
                            message_add_word(new_message, sensor_string);
                            message_add_word(new_message, "auto");
                            free(sensor_string);
                            queue_push(this_array->outgoing_monitor_msg_queue, new_message);
                        }
                        free(engine_name);
                    }
                }
            }
        }
        size_t i;
        for (i = 0; i < n_tokens; i++)
            free(tokens[i]);
        free(tokens);
    }
    array_monitor_queue_pop(this_array);
    this_array->monitor_state = ARRAY_SEND_FRONT_OF_QUEUE;
    fclose(config_file);
}


void array_handle_received_katcl_lines(struct array *this_array)
{
    while (have_katcl(this_array->control_katcl_line) > 0)
    {
        verbose_message(BORING, "From %s:%hu: %s %s %s %s %s\n", this_array->cmc_address, this_array->monitor_port, \
                arg_string_katcl(this_array->monitor_katcl_line, 0), \
                arg_string_katcl(this_array->monitor_katcl_line, 1), \
                arg_string_katcl(this_array->monitor_katcl_line, 2), \
                arg_string_katcl(this_array->monitor_katcl_line, 3), \
                arg_string_katcl(this_array->monitor_katcl_line, 4));
        char received_message_type = arg_string_katcl(this_array->control_katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, message_see_word(this_array->current_control_message, 0)))
                {
                    verbose_message(DEBUG, "%s:%hu received %s %s\n", this_array->cmc_address, this_array->control_port, \
                            message_see_word(this_array->current_control_message, 0), arg_string_katcl(this_array->control_katcl_line, 1));

                    if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 1), "ok"))
                    {
                        //Don't actually need to do anything here, the inform processing code should handle.
                    }
                    else
                    {
                        //sensor obviously doesn't exist.
                        //sensor_mark_absent(); // somehow. How will this propagate down?
                    }

                    this_array->control_state = ARRAY_SEND_FRONT_OF_QUEUE;
                    verbose_message(BORING, "%s:%hu still has %u message(s) in its queue...\n", this_array->cmc_address, \
                            this_array->control_port, queue_sizeof(this_array->outgoing_control_msg_queue));

                    if (queue_sizeof(this_array->outgoing_control_msg_queue))
                    {
                        verbose_message(BORING, "%s:%hu  popping queue...\n", this_array->cmc_address, this_array->control_port);
                        array_control_queue_pop(this_array);
                    }
                    else
                    {
                        verbose_message(BORING, "%s:%hu going into monitoring state.\n", this_array->cmc_address, this_array->control_port);
                        message_destroy(this_array->current_control_message);
                        this_array->current_control_message = NULL; //doesn't do this in the above function. C problem.
                        this_array->control_state = ARRAY_MONITOR;
                    }
                }
                break;
            case '#': // it's a katcp inform
                if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, "sensor-status"))
                {
                    verbose_message(INFO, "Received sensor-status on the control port of %s:%hu.\n", this_array->cmc_address, this_array->control_port);
                    if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 3), "instrument-state"))
                    {
                        verbose_message(INFO, "Instrument state to be updated: %s - %s\n",
                                arg_string_katcl(this_array->control_katcl_line, 5),
                                arg_string_katcl(this_array->control_katcl_line, 4));

                        if (strcmp(this_array->instrument_state, arg_string_katcl(this_array->control_katcl_line, 4)) || \
                             strcmp(this_array->config_file, arg_string_katcl(this_array->control_katcl_line, 5))  ) //without ! in front, i.e. if they are different.
                                                                        //4 is the state, 5 is the config file, so if either one changes the stuff should update.
                        {
                            if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 4), "nominal")) 
                            {
                                array_activate(this_array); 
                            }
                            free(this_array->instrument_state);
                            this_array->instrument_state = strdup(arg_string_katcl(this_array->control_katcl_line, 4));
                            free(this_array->config_file);
                            this_array->config_file = strdup(arg_string_katcl(this_array->control_katcl_line, 5));
                        }
                    }
                }
                else if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, "sensor-value"))
                {
                }
                else if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, "sensor-list"))
                {

                }
                break;
            default:
                verbose_message(WARNING, "Unexpected KATCP message received, starting with %c\n", received_message_type);
        }
    }

    while (have_katcl(this_array->monitor_katcl_line) > 0)
    {
        verbose_message(BORING, "From %s:%hu: %s %s %s %s %s\n", this_array->cmc_address, this_array->monitor_port, \
                arg_string_katcl(this_array->monitor_katcl_line, 0), \
                arg_string_katcl(this_array->monitor_katcl_line, 1), \
                arg_string_katcl(this_array->monitor_katcl_line, 2), \
                arg_string_katcl(this_array->monitor_katcl_line, 3), \
                arg_string_katcl(this_array->monitor_katcl_line, 4));
        char received_message_type = arg_string_katcl(this_array->monitor_katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (this_array->current_monitor_message)
                {
                    if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, message_see_word(this_array->current_monitor_message, 0)))
                    {
                        verbose_message(DEBUG, "%s:%hu received %s %s\n", this_array->cmc_address, this_array->monitor_port, \
                                message_see_word(this_array->current_monitor_message, 0), arg_string_katcl(this_array->monitor_katcl_line, 1));

                        if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 1), "ok"))
                        {
                            //Don't actually need to do anything here, the inform processing code should handle.
                        }
                        else
                        {
                            //sensor obviously doesn't exist.
                            //sensor_mark_absent(); // somehow. How will this propagate down?
                        }

                        this_array->monitor_state = ARRAY_SEND_FRONT_OF_QUEUE;
                        verbose_message(BORING, "%s:%hu still has %u message(s) in its queue...\n", this_array->cmc_address, \
                                this_array->monitor_port, queue_sizeof(this_array->outgoing_monitor_msg_queue));

                        if (queue_sizeof(this_array->outgoing_monitor_msg_queue))
                        {
                            verbose_message(BORING, "%s:%hu  popping queue...\n", this_array->cmc_address, this_array->monitor_port);
                            array_monitor_queue_pop(this_array);
                        }
                        else
                        {
                            verbose_message(BORING, "%s:%hu going into monitoring state.\n", this_array->cmc_address, this_array->monitor_port);
                            message_destroy(this_array->current_monitor_message);
                            this_array->current_monitor_message = NULL; //doesn't do this in the above function. C problem.
                            this_array->monitor_state = ARRAY_MONITOR;
                        }
                    }
                }
                else
                {
                    verbose_message(WARNING, "Received a katcp response %s %s %s - unexpectedly.\n", arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, arg_string_katcl(this_array->monitor_katcl_line, 1), arg_string_katcl(this_array->monitor_katcl_line, 2));
                }
                break;
            case '#': // it's a katcp inform
                if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-status"))
                {
                    //verbose_message(INFO, "Received sensor-status on the monitor port of %s:%hu.\n", this_array->cmc_address, this_array->control_port);
                    char **tokens = NULL;
                    size_t n_tokens = tokenise_string(arg_string_katcl(this_array->monitor_katcl_line, 3), '.', &tokens);
                    char team = tokens[0][0];
                    size_t team_no;
                    switch (team) {
                        case 'f': team_no = 0;
                                  break;
                        case 'x': team_no = 1;
                                  break;
                        default:  verbose_message(ERROR, "Received unknown team type %c from sensor-status message: %s\n", team, arg_string_katcl(this_array->monitor_katcl_line, 1));
                    }
                    char *host_no_str = strndup(tokens[0] + 5, 2);
                    size_t host_no = (size_t) atoi(host_no_str);
                    free(host_no_str);
                    host_no_str = NULL;

                    //Sanitise the katcl strings. Nulls cause strdup to segfault.
                    char *new_value;
                    if (arg_string_katcl(this_array->monitor_katcl_line, 5) != NULL) //I guess the "!= NULL" is redundant, but I want this to be explicit and readable
                    {
                        new_value = strdup(arg_string_katcl(this_array->monitor_katcl_line, 5));
                    }
                    else
                    {
                        verbose_message(ERROR, "KATCP message from %s:%s for sensor-status %chost%02d.%s.%s - null value received.", this_array->cmc_address, this_array->name, team, host_no, tokens[1], tokens[2]);
                        new_value = strdup("none");
                    }

                    char *new_status;
                    if (arg_string_katcl(this_array->monitor_katcl_line, 4) != NULL) 
                    {
                        new_status = strdup(arg_string_katcl(this_array->monitor_katcl_line, 4));
                    }
                    else
                    {
                        verbose_message(ERROR, "KATCP message from %s:%s for sensor-status %chost%02d.%s.%s - null status received.", this_array->cmc_address, this_array->name, team, host_no, tokens[1], tokens[2]);
                        new_status = strdup("none");
                    }

                    switch (n_tokens) {
                        case 3:
                            verbose_message(DEBUG, "Updating %chost%02d.%s.%s\n", team, host_no, tokens[1], tokens[2]);
                            team_update_sensor(this_array->team_list[team_no], host_no, tokens[1], tokens[2], new_value, new_status);
                            break;
                        case 4:
                            verbose_message(DEBUG, "Updating %chost%02d.%s.%s.%s\n", team, host_no, tokens[1], tokens[2], tokens[3]);
                            team_update_engine_sensor(this_array->team_list[team_no], host_no, tokens[1], tokens[2], tokens[3], new_value, new_status);
                            break;
                        default:
                            verbose_message(ERROR, "There was an unexpected number of tokens (%d) in the message: %s\n", n_tokens, arg_string_katcl(this_array->monitor_katcl_line, 3)); 
                    }

                    //Update the time
                    this_array->last_updated = time(0);

                    size_t i;
                    for (i = 0; i < n_tokens; i++)
                        free(tokens[i]);
                    free(tokens);
                    free(new_value);
                    free(new_status);
                }
                else if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-value"))
                {
                    //TODO
                }
                else if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-list"))
                {
                    //TODO
                }
                break;
            default:
                verbose_message(WARNING, "Unexpected KATCP message received, starting with %c\n", received_message_type);
        }
    }
}


char *array_html_summary(struct array *this_array, char *cmc_name)
{
    char format[] = "<tr><td><a href=\"%s/%s\">%s</a></td><td>%hu</td><td>%hu</td><td>%lu</td><td>%s</td><td>%s</td>";
    ssize_t needed = snprintf(NULL, 0, format, cmc_name, this_array->name, this_array->name, this_array->control_port, this_array->monitor_port, this_array->n_antennas, this_array->config_file, this_array->instrument_state) + 1;
    //TODO checks
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, cmc_name, this_array->name, this_array->name, this_array->control_port, this_array->monitor_port, this_array->n_antennas, this_array->config_file, this_array->instrument_state);
    return html_summary;
}


char *array_html_detail(struct array *this_array)
{
    char *array_detail = strdup(""); //must free() later.
    {
        char format[] = "<p align=\"right\">Last updated: %s (%d seconds ago).</p>";
        char time_str[20];
        struct tm *last_updated_tm = localtime(&this_array->last_updated);
        strftime(time_str, 20, "%F %T", last_updated_tm);
        ssize_t needed = snprintf(NULL, 0, format, time_str, (int)(time(0) - this_array->last_updated)) + 1;
        array_detail = realloc(array_detail, (size_t) needed);
        sprintf(array_detail, format, time_str, (int)(time(0) - this_array->last_updated));
    }
    
    size_t i, j;
    for (i = 0; i < this_array->n_antennas; i++)
    {
        char *row_detail = strdup("");
        for (j = 0; j < this_array->number_of_teams; j++)
        {
            char format[] = "%s%s";
            ssize_t needed = snprintf(NULL, 0, format, row_detail, team_get_host_html_detail(this_array->team_list[j], i)) + 1;
            row_detail = realloc(row_detail, (size_t) needed);
            sprintf(row_detail, format, row_detail, team_get_host_html_detail(this_array->team_list[j], i));
            verbose_message(BORING, "Row detail: %s\n", row_detail);
        }
        char format[] = "%s<tr>%s</tr>\n";
        ssize_t needed = snprintf(NULL, 0, format, array_detail, row_detail) + 1;
        array_detail = realloc(array_detail, (size_t) needed); //TODO checks
        sprintf(array_detail, format, array_detail, row_detail);
        free(row_detail);
    }

    verbose_message(BORING, "\nArray detail: %s\nstrlen: %d\n", array_detail, strlen(array_detail));
    char format[] = "<table>\n%s</table>\n";
    ssize_t needed = snprintf(NULL, 0, format, array_detail) + 1;
    char *temp = malloc((size_t) needed); //TODO I really should get around to being rigorous about this
    if (temp != NULL)
    {
        sprintf(temp, format, array_detail);
    }
    verbose_message(BORING, "\nArray detail: %s\nstrlen: %d\n", temp, strlen(temp));
    free(array_detail);
    return temp;
}


struct message *array_control_queue_pop(struct array *this_array)
{
    if (this_array->current_control_message != NULL)
    {
        message_destroy(this_array->current_control_message);
    }
    this_array->current_control_message = queue_pop(this_array->outgoing_control_msg_queue);
    return this_array->current_control_message;
}


struct message *array_monitor_queue_pop(struct array *this_array)
{
    if (this_array->current_monitor_message != NULL)
    {
        message_destroy(this_array->current_monitor_message);
    }
    this_array->current_monitor_message = queue_pop(this_array->outgoing_monitor_msg_queue);
    return this_array->current_monitor_message;
}
