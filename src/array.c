#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
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
    size_t n_antennas;

    char *cmc_address;

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
        new_array->n_antennas = n_antennas;
        new_array->cmc_address = strdup(cmc_address);

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
           this_array->team_list[this_array->number_of_teams] = team_create(team_type, this_array->n_antennas);
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
            this_array->team_list[this_array->number_of_teams] = team_create(team_type, this_array->n_antennas);
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
                verbose_message(BORING, "Sending KATCP message \"%s\" to %s:%hu\n", composed_message, this_array->cmc_address, this_array->monitor_port);
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
            fprintf(stderr, "read from %s:%hu (control) failed\n", this_array->cmc_address, this_array->control_port);
            /*TODO some kind of error checking, what to do if connection fails.*/
        }
    }

    if (FD_ISSET(this_array->control_fd, wr))
    {
        verbose_message(BORING, "Writing katcl_line to %s:%hu (control).\n", this_array->cmc_address, this_array->control_port);
        r = write_katcl(this_array->control_katcl_line);
        if (r < 0)
        {
            fprintf(stderr, "write to from %s:%hu (control) failed\n", this_array->cmc_address, this_array->control_port);
            /*TODO some kind of error checking, what to do if connection fails.*/
        }
    }

    if (FD_ISSET(this_array->monitor_fd, rd))
    {
        verbose_message(BORING, "Reading katcl_line from %s:%hu (monitor).\n", this_array->cmc_address, this_array->monitor_port);
        r = read_katcl(this_array->monitor_katcl_line);
        if (r)
        {
            fprintf(stderr, "read from %s:%hu (monitor) failed\n", this_array->cmc_address, this_array->monitor_port);
            /*TODO some kind of error checking, what to do if connection fails.*/
        }
    }

    if (FD_ISSET(this_array->monitor_fd, wr))
    {
        verbose_message(BORING, "Writing katcl_line to %s:%hu (monitor).\n", this_array->cmc_address, this_array->monitor_port);
        r = write_katcl(this_array->monitor_katcl_line);
        if (r < 0)
        {
            fprintf(stderr, "write to from %s:%hu (monitor) failed\n", this_array->cmc_address, this_array->monitor_port);
            /*TODO some kind of error checking, what to do if connection fails.*/
        }
    }
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
                        verbose_message(INFO, "%s:%hu going into monitoring state.\n", this_array->cmc_address, this_array->control_port);
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
                        free(this_array->instrument_state);
                        this_array->instrument_state = strdup(arg_string_katcl(this_array->control_katcl_line, 4));
                        free(this_array->config_file);
                        this_array->config_file = strdup(arg_string_katcl(this_array->control_katcl_line, 5));
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
                        verbose_message(INFO, "%s:%hu going into monitoring state.\n", this_array->cmc_address, this_array->monitor_port);
                        message_destroy(this_array->current_monitor_message);
                        this_array->current_monitor_message = NULL; //doesn't do this in the above function. C problem.
                        this_array->monitor_state = ARRAY_MONITOR;
                    }
                }
                break;
            case '#': // it's a katcp inform
                if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-status"))
                {

                }
                else if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-value"))
                {

                }
                else if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-list"))
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
    char format[] = "<tr><td><a href=\"%s/%s\">%s</a></td><td>%hu</td><td>%hu</td><td>%lu</td><td>%s</td><td>%s</td>";
    ssize_t needed = snprintf(NULL, 0, format, cmc_name, this_array->name, this_array->name, this_array->control_port, this_array->monitor_port, this_array->n_antennas, this_array->config_file, this_array->instrument_state) + 1;
    //TODO checks
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, cmc_name, this_array->name, this_array->name, this_array->control_port, this_array->monitor_port, this_array->n_antennas, this_array->config_file, this_array->instrument_state);
    return html_summary;
}


char *array_html_detail(struct array *this_array)
{
    char *array_html_detail = strdup(""); //must free() later.
    size_t i, j;
    for (i = 0; i < this_array->n_antennas; i++)
    {
        for (j = 0; j < this_array->number_of_teams; j++)
        {
            char format[] = "%s%s";
            ssize_t needed = snprintf(NULL, 0, format, array_html_detail, team_get_host_html_detail(this_array->team_list[j], i)) + 1;
            array_html_detail = realloc(array_html_detail, (size_t) needed);
            sprintf(array_html_detail, format, array_html_detail, team_get_host_html_detail(this_array->team_list[j], i));
        }
        char format[] = "<tr>%s</tr>\n";
        ssize_t needed = snprintf(NULL, 0, format, array_html_detail) + 1;
        array_html_detail = realloc(array_html_detail, (size_t) needed); //TODO checks
        sprintf(array_html_detail, format, array_html_detail);
    }
    return array_html_detail;
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
