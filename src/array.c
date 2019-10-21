#include <stdio.h>
#include <syslog.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <netc.h>
#include <katcp.h>
#include <katcl.h>

#include "array.h"
#include "sensor.h"
#include "team.h"
#include "message.h"
#include "queue.h"
#include "tokenise.h"

#define BUF_SIZE 1024
#define SENSOR_LIST_CONFIG_FILE "/etc/cbf_sensor_dashboard/sensor_list.conf"

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

    struct sensor **top_level_sensor_list;
    size_t num_top_level_sensors;

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

        new_array->top_level_sensor_list = NULL;
        new_array->num_top_level_sensors = 0;

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
        for (i = 0; i < this_array->num_top_level_sensors; i++)
        {
            sensor_destroy(this_array->top_level_sensor_list[i]);
        }
        free(this_array->top_level_sensor_list);

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


int array_add_top_level_sensor(struct array *this_array, char *sensor_name)
{
    this_array->top_level_sensor_list = realloc(this_array->top_level_sensor_list, \
            sizeof(*(this_array->top_level_sensor_list))*(this_array->num_top_level_sensors + 1));
    this_array->top_level_sensor_list[this_array->num_top_level_sensors] = sensor_create(sensor_name);
    this_array->num_top_level_sensors++;
    return 0; //TODO indicate failure somehow.
}


int array_update_top_level_sensor(struct array *this_array, char *sensor_name, char *new_value, char *new_status)
{
    size_t i;
    for (i = 0; i < this_array->num_top_level_sensors; i++)
    {
        if (!strcmp(sensor_name, sensor_get_name(this_array->top_level_sensor_list[i])))
        {
            return sensor_update(this_array->top_level_sensor_list[i], new_value, new_status); 
            /// \retval 0 The operation was successful.
        }
    }
    return -1; /// \retval -1 The operation failed.
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
                syslog(LOG_WARNING, "A message on %s:%hu's (control) queue had 0 words in it. Cannot send.", this_array->cmc_address, this_array->control_port);
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
                syslog(LOG_WARNING, "A message on %s:%hu's (monitor) queue had 0 words in it. Cannot send.", this_array->cmc_address, this_array->monitor_port);
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
        r = read_katcl(this_array->control_katcl_line);
        if (r)
        {
            syslog(LOG_ERR, "Read from %s:%hu (control) failed.", this_array->cmc_address, this_array->control_port);
            this_array->control_state = ARRAY_DISCONNECTED;
        }
    }

    if (FD_ISSET(this_array->control_fd, wr))
    {
        r = write_katcl(this_array->control_katcl_line);
        if (r < 0)
        {
            syslog(LOG_ERR, "Write to %s:%hu (control) failed.", this_array->cmc_address, this_array->control_port);
            this_array->control_state = ARRAY_DISCONNECTED;
        }
    }

    if (FD_ISSET(this_array->monitor_fd, rd))
    {
        r = read_katcl(this_array->monitor_katcl_line);
        if (r)
        {
            syslog(LOG_ERR, "Read from %s:%hu (monitor) failed.", this_array->cmc_address, this_array->monitor_port);
            this_array->monitor_state = ARRAY_DISCONNECTED;
        }
    }

    if (FD_ISSET(this_array->monitor_fd, wr))
    {
        r = write_katcl(this_array->monitor_katcl_line);
        if (r < 0)
        {
            syslog(LOG_ERR, "Write to from %s:%hu (monitor) failed.", this_array->cmc_address, this_array->monitor_port);
            this_array->monitor_state = ARRAY_DISCONNECTED;
        }
    }
}


static void array_activate(struct array *this_array)
{
    syslog(LOG_NOTICE, "Detected %s:%s in nominal state, subscribing to sensors.", this_array->cmc_address, this_array->name);
    FILE *config_file = fopen(SENSOR_LIST_CONFIG_FILE, "r");

    char buffer[BUF_SIZE];
    char *result;

    {
        struct message *new_message = message_create('?');
        message_add_word(new_message, "sensor-value");
        message_add_word(new_message, "hostname-functional-mapping");
        queue_push(this_array->outgoing_monitor_msg_queue, new_message);

        new_message = message_create('?');
        message_add_word(new_message, "sensor-value");
        message_add_word(new_message, "input-labelling");
        queue_push(this_array->outgoing_control_msg_queue, new_message);
     } ///TODO figure out some way to deal with this!

    //This needs to be hardcoded unfortunately.
    array_add_top_level_sensor(this_array, "device-status");
    struct message *new_message = message_create('?');
    message_add_word(new_message, "sensor-sampling");
    message_add_word(new_message, "device-status");
    message_add_word(new_message, "auto");
    queue_push(this_array->outgoing_monitor_msg_queue, new_message);
    
    //Subscribe to sensors configured in the config file.
    for (result = fgets(buffer, BUF_SIZE, config_file); result != NULL; result = fgets(buffer, BUF_SIZE, config_file))
    {
        char **tokens = NULL;
        size_t n_tokens = tokenise_string(buffer, '.', &tokens);
        if (!((n_tokens == 1) || (n_tokens == 2) || (n_tokens == 3)))
            //can't think of a better way to put it than this.i
            //one token means top-level array sensor,
            //two means team, host, device,
            //three means team, host, engine, device
        {
            syslog(LOG_ERR, "sensor_list.conf has a malformed sensor name: %s.", buffer);
        }
        else
        {
            char team_type = tokens[0][0]; //should be just "f" or "x" at this point.
            size_t i;
            switch (n_tokens) {
                case 1:
                    {
                        array_add_top_level_sensor(this_array, tokens[0]);
                        struct message *new_message = message_create('?');
                        message_add_word(new_message, "sensor-sampling");
                        message_add_word(new_message, tokens[0]);
                        message_add_word(new_message, "auto");
                        queue_push(this_array->outgoing_monitor_msg_queue, new_message);
                    }
                    break;
                case 2:
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
                    break;
                case 3:
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
                    break;
                default:
                    ; //won't get here, checked earlier.
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
        char received_message_type = arg_string_katcl(this_array->control_katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, message_see_word(this_array->current_control_message, 0)))
                {
                    if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 1), "ok"))
                    {
                        //Don't actually need to do anything here, the inform processing code should handle.
                    }
                    else
                    {
                        //sensor obviously doesn't exist.
                        //sensor_mark_absent(); // TODO somehow. How will this propagate down? -- this is needed to indicate that there's something wrong in the config file.
                    }

                    this_array->control_state = ARRAY_SEND_FRONT_OF_QUEUE;

                    if (queue_sizeof(this_array->outgoing_control_msg_queue))
                    {
                        array_control_queue_pop(this_array);
                    }
                    else
                    {
                        syslog(LOG_DEBUG, "%s:%hu going into monitoring state.", this_array->cmc_address, this_array->control_port);
                        message_destroy(this_array->current_control_message);
                        this_array->current_control_message = NULL; //doesn't do this in the above function. C problem.
                        this_array->control_state = ARRAY_MONITOR;
                    }
                }
                break;
            case '#': // it's a katcp inform
                if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, "sensor-status"))
                {
                    if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 3), "instrument-state"))
                    {
                        //TODO consider copying these things into their own strings to make for a bit more clarity.
                        syslog(LOG_NOTICE, "%s (%s) Instrument state to be updated: %s - %s",
                                this_array->name, this_array->cmc_address,
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
                    //TODO handle input-labelling here
                }
                else if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, "sensor-list"))
                {
                }
                break;
            default:
                ; //This shouldn't ever happen.
        }
    }

    while (have_katcl(this_array->monitor_katcl_line) > 0)
    {
        char received_message_type = arg_string_katcl(this_array->monitor_katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (this_array->current_monitor_message)
                {
                    if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, message_see_word(this_array->current_monitor_message, 0)))
                    {
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

                        if (queue_sizeof(this_array->outgoing_monitor_msg_queue))
                        {
                            array_monitor_queue_pop(this_array);
                        }
                        else
                        {
                            syslog(LOG_DEBUG, "%s:%hu going into monitoring state.", this_array->cmc_address, this_array->monitor_port);
                            message_destroy(this_array->current_monitor_message);
                            this_array->current_monitor_message = NULL; //doesn't do this in the above function. C problem.
                            this_array->monitor_state = ARRAY_MONITOR;
                        }
                    }
                }
                else
                {
                    syslog(LOG_WARNING, "Received a katcp response %s %s %s - unexpectedly.", arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, arg_string_katcl(this_array->monitor_katcl_line, 1), arg_string_katcl(this_array->monitor_katcl_line, 2));
                }
                break;
            case '#': // it's a katcp inform
                if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-status"))
                {
                    char **tokens = NULL;
                    size_t n_tokens = tokenise_string(arg_string_katcl(this_array->monitor_katcl_line, 3), '.', &tokens);
                    char team = tokens[0][0];
                    size_t team_no;
                    switch (team) {
                        case 'f': team_no = 0;
                                  break;
                        case 'x': team_no = 1;
                                  break;
                        default:  syslog(LOG_WARNING, "Received unknown team type %c from sensor-status message: %s", team, arg_string_katcl(this_array->monitor_katcl_line, 1));
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
                        syslog(LOG_ERR, "KATCP message from %s:%s for sensor-status %chost%02ld.%s.%s - null value received.", this_array->cmc_address, this_array->name, team, host_no, tokens[1], tokens[2]);
                        new_value = strdup("none");
                    }

                    char *new_status;
                    if (arg_string_katcl(this_array->monitor_katcl_line, 4) != NULL) 
                    {
                        new_status = strdup(arg_string_katcl(this_array->monitor_katcl_line, 4));
                    }
                    else
                    {
                        syslog(LOG_ERR, "KATCP message from %s:%s for sensor-status %chost%02ld.%s.%s - null status received.", this_array->cmc_address, this_array->name, team, host_no, tokens[1], tokens[2]);
                        new_status = strdup("none");
                    }

                    switch (n_tokens) {
                        case 1:
                            array_update_top_level_sensor(this_array, tokens[0], new_value, new_status);
                            break;
                        case 3:
                            team_update_sensor(this_array->team_list[team_no], host_no, tokens[1], tokens[2], new_value, new_status);
                            break;
                        case 4:
                            team_update_engine_sensor(this_array->team_list[team_no], host_no, tokens[1], tokens[2], tokens[3], new_value, new_status);
                            break;
                        default:
                            //TODO make this error message a bit more reasonable so that I'd be able to find it if I needed to.
                            syslog(LOG_ERR, "There was an unexpected number of tokens (%ld) in the message: %s", n_tokens, arg_string_katcl(this_array->monitor_katcl_line, 3)); 
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
                    if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 3), "hostname-functional-mapping") && arg_string_katcl(this_array->monitor_katcl_line, 5) != NULL)
                    {
                        char *sensor_value = strdup(arg_string_katcl(this_array->monitor_katcl_line, 5));
                        syslog(LOG_INFO, "(%s:%s) Received hostname-functional-mapping: %s", this_array->cmc_address, this_array->name, sensor_value);
                        int i;
                        for (i = 0; i < 2*this_array->n_antennas; i++) //hacky.
                        {
                            if (i*30 + 21 > strlen(sensor_value))
                                    break;
                            char host_type = sensor_value[i*30 + 21];
                            syslog(LOG_DEBUG, "host type: %c", host_type);

                            char *host_number_str = strndup(sensor_value + (i*30 + 26), 2);
                            size_t host_number = (size_t) atoi(host_number_str);
                            syslog(LOG_DEBUG, "host number: %lu", host_number);
                            char *host_serial = strndup(sensor_value + (i*30 + 8), 6);
                            syslog(LOG_DEBUG, "host serial: %s", host_serial);
                            switch (host_type)
                            {
                                //TODO: this should probably check more rigorously against team types.
                                case 'f':
                                    team_set_host_serial_no(this_array->team_list[0], host_number, host_serial);
                                    break;
                                case 'x':
                                    team_set_host_serial_no(this_array->team_list[1], host_number, host_serial);
                                    break;
                                default:
                                    syslog(LOG_WARNING, "Couldn't properly parse hostname-functional-mapping for %s:%s.", \
                                            this_array->cmc_address, this_array->name);
                            }
                            free(host_serial);
                            free(host_number_str);
                        }
                        free(sensor_value);
                    }
                }
                else if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-list"))
                {
                    //TODO
                }
                break;
            default:
                ; //This shouldn't ever happen.
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

    char *top_detail = strdup("");
    {
        //collect the top-level sensors first.
        size_t i;
        char *tl_sensors_rep = strdup("");
        for (i = 0; i < this_array->num_top_level_sensors; i++)
        {
            char tl_sensors_format[] = "%s<button class=\"%s\" style=\"width:300px\">%s</button> ";
            ssize_t needed = snprintf(NULL, 0, tl_sensors_format, tl_sensors_rep, sensor_get_status(this_array->top_level_sensor_list[i]), \
                    sensor_get_name(this_array->top_level_sensor_list[i])) + 1;
            tl_sensors_rep = realloc(tl_sensors_rep, (size_t) needed); //TODO check for -1
            sprintf(tl_sensors_rep, tl_sensors_format, tl_sensors_rep, sensor_get_status(this_array->top_level_sensor_list[i]), \
                    sensor_get_name(this_array->top_level_sensor_list[i]));
        }
        char format[] = "<p align=\"right\">%s Last updated: %s (%d seconds ago).</p>";
        char time_str[20];
        struct tm *last_updated_tm = localtime(&this_array->last_updated);
        strftime(time_str, 20, "%F %T", last_updated_tm);
        ssize_t needed = snprintf(NULL, 0, format, tl_sensors_rep, time_str, (int)(time(0) - this_array->last_updated)) + 1;
        top_detail = realloc(top_detail, (size_t) needed);
        sprintf(top_detail, format, tl_sensors_rep, time_str, (int)(time(0) - this_array->last_updated));
        free(tl_sensors_rep);
    }
    
    char *array_detail = strdup(""); //must free() later.
    size_t i, j;
    for (i = 0; i < this_array->n_antennas; i++)
    {
        char *row_detail = strdup("");
        for (j = 0; j < this_array->number_of_teams; j++)
        {
            char format[] = "%s%s";
            char *host_html_det = team_get_host_html_detail(this_array->team_list[j], i);
            ssize_t needed = snprintf(NULL, 0, format, row_detail, host_html_det) + 1;
            row_detail = realloc(row_detail, (size_t) needed);
            sprintf(row_detail, format, row_detail, host_html_det);
            free(host_html_det);
        }
        char format[] = "%s<tr>%s</tr>\n";
        ssize_t needed = snprintf(NULL, 0, format, array_detail, row_detail) + 1;
        array_detail = realloc(array_detail, (size_t) needed); //TODO checks
        sprintf(array_detail, format, array_detail, row_detail);
        free(row_detail);
    }

    char format[] = "%s\n<table>\n%s</table>\n";
    ssize_t needed = snprintf(NULL, 0, format, top_detail, array_detail) + 1;
    char *temp = malloc((size_t) needed); //TODO I really should get around to being rigorous about this
    if (temp != NULL)
    {
        sprintf(temp, format, top_detail, array_detail);
    }
    free(top_detail);
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
