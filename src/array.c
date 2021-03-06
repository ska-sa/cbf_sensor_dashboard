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


/// A struct representing all the info you need to store data from an array and communicate with the correlator itself.
struct array {
    /// The name of the array.
    char *name;
    /// An indicator of whether the array is active or not, whether it's in need of garbage collection.
    int array_is_active;

    /// A list of top-level sensors, which do not belong to any host.
    struct sensor **top_level_sensor_list;
    /// The number of top-level sensors in the list.
    size_t num_top_level_sensors;

    /// A list of teams of *hosts. Currently there are only 'f' and 'x'.
    struct team **team_list;
    /// The number of teams in the list, currently this will always be 2.
    size_t number_of_teams;
    /// The number of antennas to which this array can connect (i.e. the size of the correlator).
    size_t n_antennas;
    /// The number of xhosts. This may be different from the number of antennas in the narrowband case.
    size_t n_xhosts;

    /// The IP address or (resolvable) hostname of the CMC which is controlling the correlator.
    char *cmc_address;

    /// The time at which the most recent information was received from the array. Useful as a debug indicator of whether the connection is still alive.
    time_t last_updated;

    /// The TCP port at which the correlator's corr2_servlet is listening for KATCP connections.
    uint16_t control_port;
    /// A file descriptor to handle the connection to the corr2_servlet.
    int control_fd;
    /// A katcl_line to handle the KATCP communication through this connection.
    struct katcl_line *control_katcl_line;
    /// The current state of the state machine for this communication.
    enum array_state control_state;
    /// The queue for messages waiting to be sent to the corr2_servlet.
    struct queue *outgoing_control_msg_queue;
    /// The most recently sent message.
    struct message *current_control_message;

    /// The overall instrument status.
    char *instrument_state;
    /// The config file from which the instrument was launched. Indicates what specific configuration the correlator is in.
    char *config_file;

    /// The TCP port at which the correlator's corr2_sensor_servlet is listening for KATCP connections.
    uint16_t monitor_port;
    /// A file descriptor to handle the connection to the corr2_sensor_servlet.
    int monitor_fd;
    /// A katcl_line to handle the KATCP communication through this connection.
    struct katcl_line *monitor_katcl_line;
    /// The current state of the state machine for this communication.
    enum array_state monitor_state;
    /// The queue for messages waiting to be sent to the corr2_sensor_servlet.
    struct queue *outgoing_monitor_msg_queue;
    /// The most recently sent message.
    struct message *current_monitor_message;

    /// Stores whether or not we have received the hostname-functional-mapping for the array, helps save time.
    int hostname_functional_mapping_received;
};


/**
 * \fn      struct array *array_create(char *new_array_name, char *cmc_address, uint16_t control_port, uint16_t monitor_port, size_t n_antennas)
 * \details Allocate memory for a new array object, create teams with hosts, queue up a few messages to send.
 * \param   new_array_name A string containing the name for the new array.
 * \param   cmc_address A string containing the IP or (resolvable) hostname of the CMC server.
 * \param   control_port The TCP port that the correlator's corr2_servlet is listening to.
 * \param   monitor_port The TCP port that the correlator's corr2_sensor_servlet is listening to.
 * \param   n_antennas The number of antennas, or the size of the correlator.
 * \return  A pointer to the newly-allocated array object.
 */
struct array *array_create(char *new_array_name, char *cmc_address, uint16_t control_port, uint16_t monitor_port, size_t n_antennas)
{
   struct array *new_array = malloc(sizeof(*new_array));
   if (new_array != NULL)
   {
        new_array->name = strdup(new_array_name);
        new_array->array_is_active = 1;
        new_array->n_antennas = n_antennas;
        new_array->n_xhosts = 0; // We don't know this in advance.
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

        new_array->hostname_functional_mapping_received = 0;
   }
   return new_array;
}


/**
 * \fn      void array_destroy(struct array *this_array)
 * \details Free the memory associated with the array and all its children.
 * \param   this_array A pointer to the array to be destroyed.
 * \return  void
 */
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


/**
 * \fn      char *array_get_name(struct array *this_array)
 * \details Get the name of the array.
 * \param   this_array A pointer to the array in question.
 * \return  A string containing the name of the array. This string is not newly-allocated, and must not be freed.
 */
char *array_get_name(struct array *this_array)
{
    return this_array->name;
}


/**
 * \fn      int array_get_size(struct array *this_array)
 * \details Get the size (i.e. number of antennas) of the array.
 * \param   this_array A pointer to the array in question.
 * \return  The (size_t) number of antennas in the array.
 */
size_t array_get_size(struct array *this_array)
{
    return this_array->n_antennas;
}


/**
 * \fn      int array_add_team_host_device_sensor(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name)
 * \details Add a sensor to the array. Parent structures will be created if necessary.
 * \param   this_array A pointer to the array in question.
 * \param   team_type The type of host that the sensor will be on ('f' or 'x').
 * \param   host_number The index of the host in the team.
 * \param   device_name A string containing the name of the device that will contain the sensor.
 * \param   sensor_name A string containing the name of the sensor to be created.
 * \return  An integer indicating the outcome of the operation.
 */
int array_add_team_host_device_sensor(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name)
{
    //syslog(LOG_DEBUG, "Adding sensor %chost%lu.%s.%s to %s:%s.", team_type, host_number, device_name, sensor_name, this_array->cmc_address, this_array->name);
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


/**
 * \fn      int array_add_team_host_engine_device_sensor(struct array *this_array, char team_type, size_t host_number, char *engine_name, char *device_name, char *sensor_name)
 * \details Add a sensor to the array. Parent structures will be created if necessary.
 * \param   this_array A pointer to the array in question.
 * \param   team_type The type of host that the sensor will be on ('f' or 'x').
 * \param   host_number The index of the host in the team.
 * \param   engine_name A string containing the name of the engine that will contain the device.
 * \param   device_name A string containing the name of the device that will contain the sensor.
 * \param   sensor_name A string containing the name of the sensor to be created.
 * \return  An integer indicating the outcome of the operation.
 */
int array_add_team_host_engine_device_sensor(struct array *this_array, char team_type, size_t host_number, char *engine_name, char *device_name, char *sensor_name)
{
    //syslog(LOG_DEBUG, "Adding sensor %chost%lu.%s.%s.%s to %s:%s.", team_type, host_number, engine_name, device_name, sensor_name, this_array->cmc_address, this_array->name);
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


/**
 * \fn      int array_add_top_level_sensor(struct array *this_array, char *sensor_name)
 * \details Add a top-level sensor to the array.
 * \param   this_array A pointer to the array in question.
 * \param   sensor_name A string with the intended name for the new sensor.
 * \return  An integer indicating the outcome, at the moment this is coded always to return 0.
 */
int array_add_top_level_sensor(struct array *this_array, char *sensor_name)
{
    syslog(LOG_DEBUG, "Top-level sensor %s added to %s:%s.", sensor_name, this_array->cmc_address, this_array->name);
    this_array->top_level_sensor_list = realloc(this_array->top_level_sensor_list, \
            sizeof(*(this_array->top_level_sensor_list))*(this_array->num_top_level_sensors + 1));
    this_array->top_level_sensor_list[this_array->num_top_level_sensors] = sensor_create(sensor_name);
    this_array->num_top_level_sensors++;
    return 0; //TODO indicate failure somehow.
}


/**
 * \fn      int array_update_top_level_sensor(struct array *this_array, char *sensor_name, char *new_value, char *new_status)
 * \details Update the value and status of a top-level sensor in the array.
 * \param   this_array A pointer to the array in question.
 * \param   sensor_name A string containing the name of the sensor to update.
 * \param   new_value A string containing the new value to write to the sensor.
 * \param   new_status A string containing the new status to write to the sensor.
 * \return  An integer indicating the outcome of the operation.
 */
int array_update_top_level_sensor(struct array *this_array, char *sensor_name, char *new_value, char *new_status)
{
    syslog(LOG_DEBUG, "Top-level sensor %s in %s:%s updated with %s - %s", sensor_name, this_array->cmc_address, this_array->name, new_value, new_status);
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


/**
 * \fn      void array_mark_suspect(struct array *this_array)
 * \details Mark the array as possibly dead. If its name comes back in an #array-list, then it'll be cleared, otherwise it'll be presumed dead
 *          and garbage-collected.
 * \param   this_array A pointer to the array in question.
 * \return  void
 */
void array_mark_suspect(struct array *this_array)
{
    this_array->array_is_active = 0;
}


/**
 * \fn      int array_check_suspect(struct array *this_array)
 * \details Check whether the array is suspect.
 * \param   this_array A pointer to the array in question.
 * \return  0 if it's okay, 1 if it's suspect, -1 if the pointer is null.
 */
int array_check_suspect(struct array *this_array)
{
    if (this_array != NULL)
        return !this_array->array_is_active; //i.e. it's not suspect if it's active.
    else
        return -1;
}


/**
 * \fn      void array_mark_fine(struct array *this_array)
 * \details Mark the array as fine. Also check whether there are any stagnant sensors under the array, and
 *          re-request them as necessary.
 * \param   this_array A pointer to the array in question.
 * \return  void
 */
void array_mark_fine(struct array *this_array)
{
    this_array->array_is_active = 1;

    if (this_array->last_updated + 60 > time(0))
    {
        struct message *new_message = message_create('?');
        message_add_word(new_message, "sensor-value");
        queue_push(this_array->outgoing_monitor_msg_queue, new_message);

        if (this_array->monitor_state == ARRAY_MONITOR)
        {
            array_monitor_queue_pop(this_array);
            this_array->monitor_state = ARRAY_SEND_FRONT_OF_QUEUE;
        }
    }

    //request sensor-value stagnant sensors just in case we missed something.
    /*size_t n_stagnant_sensors = 0;
    char **stagnant_sensors = array_get_stagnant_sensor_names(this_array, 120, &n_stagnant_sensors);

    size_t i;
    for (i = 0; i < n_stagnant_sensors; i++)
    {
        if (queue_sizeof(this_array->outgoing_monitor_msg_queue) > MESSAGE_BUFFER_MAX_LENGTH)
        {
            struct message *new_message = message_create('?');
            message_add_word(new_message, "sensor-value");
            message_add_word(new_message, stagnant_sensors[i]);
            queue_push(this_array->outgoing_monitor_msg_queue, new_message);

            //syslog(LOG_DEBUG, "%s:%s monitor queue now has %zd messages waiting.",
            //        this_array->cmc_address, this_array->name, queue_sizeof(this_array->outgoing_monitor_msg_queue));

            if (this_array->monitor_state == ARRAY_MONITOR)
            {
                array_monitor_queue_pop(this_array);
                this_array->monitor_state = ARRAY_SEND_FRONT_OF_QUEUE;
            }
        }
        else
        {
            syslog(LOG_DEBUG, "Too many stagnant sensors on %s:%s monitor queue, dropping the remaining %zd.",
                    this_array->cmc_address, this_array->name, n_stagnant_sensors - i);
        }
    }

    for (i = 0; i < n_stagnant_sensors; i++)
        free(stagnant_sensors[i]);
    free(stagnant_sensors);*/
}


/**
 * \fn      int array_functional(struct array *this_array)
 * \details Check whether the array is functional. An array is not functional if either of the KATCP connections is disconnected.
 * \param   this_array A pointer to the array in question.
 * \return  1 if both connections are active, -1 if either isn't active.
 */
int array_functional(struct array *this_array)
{
    if (this_array->control_state == ARRAY_DISCONNECTED || this_array->monitor_state == ARRAY_DISCONNECTED)
        return -1;
    else
        return 1;
}


/**
 * \fn      char *array_get_sensor_value(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name)
 * \details Get the value of a sensor from the array.
 * \param   this_array A pointer to the array in question.
 * \param   team_type The type of host which holds the sensor.
 * \param   host_number The index of the host in the team.
 * \param   device_name A string containing the name of the device which contains the sensor.
 * \param   sensor_name A string containing the name of the sensor to be queried.
 * \return  A string containing the value of the queried sensor.
 */
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


/**
 * \fn      char *array_get_sensor_status(struct array *this_array, char team_type, size_t host_number, char *device_name, char *sensor_name)
 * \details Get the value of a sensor from the array.
 * \param   this_array A pointer to the array in question.
 * \param   team_type The type of host which holds the sensor.
 * \param   host_number The index of the host in the team.
 * \param   device_name A string containing the name of the device which contains the sensor.
 * \param   sensor_name A string containing the name of the sensor to be queried.
 * \return  A string containing the status of the queried sensor.
 */
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


/**
 * \fn      void array_set_fds(struct array *this_array, fd_set *rd, fd_set *wr, int *nfds)
 * \details This function sets both read and write file desciptors according to the state that the array's state machines are in.
 *          the rd file descriptor will almost always be set if the connection is active, with the wr file descriptor set only if there
 *          is a message waiting to be sent.
 * \param   this_array A pointer to the array in question.
 * \param   rd A pointer to the fd_set indicating ready to read.
 * \param   wr A pointer to the fd_set indicating ready to write.
 * \param   nfds A pointer to an integer indicating the number of file descriptors in the above sets.
 * \return  void
 */
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


/**
 * \fn      void array_setup_katcp_writes(struct array *this_array)
 * \details If there is a message waiting to be sent, this function will insert it into the katcl_line, word for word, until it's finished.
 *          On the next select() loop, the katcl_line will then report that it's ready to write a fully-formed message to the file descriptor.
 * \param   this_array pointer to the array in question.
 * \return  void
 */
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


/**
 * \fn      void array_socket_read_write(struct array *this_array, fd_set *rd, fd_set *wr)
 * \details Depending on the state that the array's state machines are in, send all transmissions which are ready, and read
 *          incoming transmissions, storing them in the katcl_line for processing once a fully-formed message is received.
 * \param   this_array A pointer to the array in question.
 * \param   rd A pointer to the fd_set indicating ready to read.
 * \param   wr A pointer to the fd_set indicating ready to write.
 * \return  void
 */
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


/**
 * \fn      static void array_activate(struct array *this_array)
 * \details Activate the array. The array will appear on the array-list before it's ready to be connected and probed for all its sensor data.
 *          Once the array is ready, this function makes all the connections, sets the state-machines up, and gets things going.
 * \param   this_array A pointer to the array in question.
 * \return  void
 */
static void array_activate(struct array *this_array)
{
    //if (strstr(this_array->name, "narrow"))
    //    return;
    syslog(LOG_NOTICE, "Detected %s:%s in nominal state, subscribing to sensors.", this_array->cmc_address, this_array->name);
    FILE *config_file = fopen(SENSOR_LIST_CONFIG_FILE, "r");

    char buffer[BUF_SIZE];
    char *result;

    {
        struct message *new_message = message_create('?');
        message_add_word(new_message, "sensor-sampling");
        message_add_word(new_message, "hostname-functional-mapping");
        message_add_word(new_message, "auto");
        queue_push(this_array->outgoing_monitor_msg_queue, new_message);

        new_message = message_create('?');
        message_add_word(new_message, "sensor-sampling");
        message_add_word(new_message, "input-labelling");
        message_add_word(new_message, "auto");
        queue_push(this_array->outgoing_control_msg_queue, new_message);

        new_message = message_create('?');
        message_add_word(new_message, "sensor-value");
        message_add_word(new_message, "n-xeng-hosts");
        queue_push(this_array->outgoing_control_msg_queue, new_message);
     }

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

                            char format[] = "%chost%02lu.%s.device-status"; 
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
                    if (strstr(tokens[1], "eng")) //i.e. the second token contains the word "eng" - so we assume host.eng.device.device-status
                    {                             // with the "device-status" part being implicit.
                        for (i = 0; i < this_array->n_antennas; i++)
                        {
                            size_t j;
                            for (j = 0; j < 4; j++)
                            {
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
                    else //i.e. we assume host.device.sensor
                         //NB! For the time being, the only case of this is xhost.missing-pkts.fhost%02d-cnt.
                         //so I'm going to semi-hardcode for that. If another case is required, I'm going to have to format this somewhat differently.
                    {
                        for (i = 0; i < this_array->n_antennas; i++)
                        {
                            int j;
                            for (j = 0; j < this_array->n_antennas; j++) //each xengine gets packets from each fengine
                            {
                                ssize_t needed = snprintf(NULL, 0, tokens[2], j) + 1;
                                char *sensor_name = malloc((size_t) needed);
                                sprintf(sensor_name, tokens[2], j);

                                array_add_team_host_device_sensor(this_array, team_type, i, tokens[1], sensor_name);

                                char format[] = "%chost%02lu.%s.%s";
                                needed = snprintf(NULL, 0, format, team_type, i, tokens[1], sensor_name) + 1;
                                char *sensor_string = malloc((size_t) needed);
                                sprintf(sensor_string, format, team_type, i, tokens[1], sensor_name);
                                free(sensor_name);
                                struct message *new_message = message_create('?');
                                message_add_word(new_message, "sensor-sampling");
                                message_add_word(new_message, sensor_string);
                                message_add_word(new_message, "auto");
                                free(sensor_string);
                                queue_push(this_array->outgoing_monitor_msg_queue, new_message);
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
    if (this_array->control_state == ARRAY_MONITOR)
    {
        array_control_queue_pop(this_array); 
        this_array->control_state = ARRAY_SEND_FRONT_OF_QUEUE;
    } //If it's not in ARRAY_MONITOR state, it means that we're probably activating the array straight away on starting the server.
      //but if we've been waiting around for a while, we need to get things ticking over a bit.
    fclose(config_file);
}


/**
 * \fn      void array_handle_received_katcl_lines(struct array *this_array)
 * \details This function checks whether the katcl_line has any messages ready, and then processes the message, in accordance with the logic of the
 *          built-in state machine.
 * \param   this_array A pointer to the array in question.
 * \return  void
 */
void array_handle_received_katcl_lines(struct array *this_array)
{
    while (have_katcl(this_array->control_katcl_line) > 0)
    {
	//syslog(LOG_DEBUG, "Receved katcp message on %s:%s (control) - %s %s %s %s %s", this_array->cmc_address, this_array->name,
	//		arg_string_katcl(this_array->control_katcl_line, 0),
	//		arg_string_katcl(this_array->control_katcl_line, 1),
	//		arg_string_katcl(this_array->control_katcl_line, 2),
	//		arg_string_katcl(this_array->control_katcl_line, 3),
	//		arg_string_katcl(this_array->control_katcl_line, 4));

        char received_message_type = arg_string_katcl(this_array->control_katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, message_see_word(this_array->current_control_message, 0)))
                {
                    char *composed_message = message_compose(this_array->current_control_message);
                    if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 1), "ok"))
                    {
                        //Don't actually need to do anything here, the inform processing code should handle.
                        //syslog(LOG_DEBUG, "Waiting message was: %s", composed_message);
                    }
                    else
                    {
                        //If sensor value requests are failing, it could be that we are looking for xhosts that don't exist.
                        char *r;
                        if ((r = strstr(composed_message, "xhost")))
                        {
                            char *xhost_n_chr = strndup(r, 2);
                            int xhost_n = atoi(xhost_n_chr);
                            free(xhost_n_chr);
                            if (xhost_n < this_array->n_xhosts)
                            {
                                //If we get too many of these, there is something wrong.
                                syslog(LOG_WARNING, "(%s:%s) Fail response to [%s] received. Queue resending...", this_array->cmc_address, this_array->name, composed_message);
                                queue_push(this_array->outgoing_control_msg_queue, this_array->current_control_message);
                            }
                            else
                            {
                                syslog(LOG_INFO, "(%s:%s) Fail response to [%s] received. NOT resending, probably narrowband.", this_array->cmc_address, this_array->name, composed_message);
                                message_destroy(this_array->current_control_message);
                            }
                        }
                        else
                            queue_push(this_array->outgoing_control_msg_queue, this_array->current_control_message);
                        this_array->current_control_message = NULL;
                    }
                    free(composed_message);

                    if (queue_sizeof(this_array->outgoing_control_msg_queue))
                    {
                        array_control_queue_pop(this_array);
                        this_array->control_state = ARRAY_SEND_FRONT_OF_QUEUE;
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
                            if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 4), "nominal") && strcmp(arg_string_katcl(this_array->control_katcl_line, 5), "none"))
                                                                                                            //i.e. the config file is not none. Otherwise array_activate potentially
                                                                                                            //gets run twice.
                            {
                                array_activate(this_array); 
                            }
                            free(this_array->instrument_state);
                            this_array->instrument_state = strdup(arg_string_katcl(this_array->control_katcl_line, 4));
                            free(this_array->config_file);
                            this_array->config_file = strdup(arg_string_katcl(this_array->control_katcl_line, 5));
                        }
                    }
                    else if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 3), "input-labelling"))
                    {
                        if (arg_string_katcl(this_array->control_katcl_line, 5) != NULL)
                        {
                            char *sensor_value = strdup(arg_string_katcl(this_array->control_katcl_line, 5));
                            syslog(LOG_INFO, "(%s:%s) Received input-labelling: %s", this_array->cmc_address, this_array->name, sensor_value);
                            
                            //hacky. No fixed width fields, but we can tokenise stuff and get it in the correct order.
                            size_t i = 0;
                            char *temp;
                            char delims[] = "[(' ,)]";
                            do {
                                if (i == 0)
                                    temp = strtok(sensor_value, delims);
                                else 
                                    temp = strtok(NULL, delims);
                                team_set_fhost_input_stream(this_array->team_list[0], temp, i);
                                //don't need the next seven values.
                                strtok(NULL, delims);
                                strtok(NULL, delims);
                                strtok(NULL, delims);
                                strtok(NULL, delims);
                                strtok(NULL, delims);
                                strtok(NULL, delims);
                                strtok(NULL, delims);
                            } while (++i < this_array->n_antennas);

                            free(sensor_value);
                        }
                        else
                        {
                            //we get quite a few of these, not even worried about them anymore.
                            syslog(LOG_DEBUG, "(%s:%s) Received NULL input-labelling!", this_array->cmc_address, this_array->name);
                        }
                    }
                }
                else if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 0) + 1, "sensor-value"))
                {
                    if (arg_string_katcl(this_array->control_katcl_line, 5) != NULL)
                    {
                        if (!strcmp(arg_string_katcl(this_array->control_katcl_line, 3), "n-xeng-hosts"))
                        {
                            this_array->n_xhosts = (size_t) atoi(arg_string_katcl(this_array->control_katcl_line, 5));
                        }
                    }
                    else
                    {
                        //not even a warning, apparently this is expected behaviour. Thanks CAM.
                        syslog(LOG_DEBUG, "(%s:%s) Received NULL sensor-value!", this_array->cmc_address, this_array->name);
                    }
                }
                break;
            default:
                ; //This shouldn't ever happen.
        }
    }

    while (have_katcl(this_array->monitor_katcl_line) > 0)
    {
        char received_message_type = arg_string_katcl(this_array->monitor_katcl_line, 0)[0];

	//syslog(LOG_DEBUG, "Receved katcp message on %s:%s (monitor) - %s %s %s %s %s", this_array->cmc_address, this_array->name,
	//		arg_string_katcl(this_array->monitor_katcl_line, 0),
	//		arg_string_katcl(this_array->monitor_katcl_line, 1),
	//		arg_string_katcl(this_array->monitor_katcl_line, 2),
	//		arg_string_katcl(this_array->monitor_katcl_line, 3),
	//		arg_string_katcl(this_array->monitor_katcl_line, 4));

        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, message_see_word(this_array->current_monitor_message, 0)))
                {
                    char *composed_message = message_compose(this_array->current_monitor_message);
                    if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 1), "ok"))
                    {
                        //Don't actually need to do anything here, the inform processing code should handle.
                        //syslog(LOG_DEBUG, "Waiting message was: %s", composed_message);
                    }
                    else
                    {
                        //If sensor value requests are failing, it could be that we are looking for xhosts that don't exist.
                        char *r;
                        if ((r = strstr(composed_message, "xhost")))
                        {
                            char *xhost_n_chr = strndup(r + strlen("xhost"), 2);
                            int xhost_n = atoi(xhost_n_chr);
                            //syslog(LOG_WARNING, "Noticed that an xhost message failed. We have %zd xhosts in this array, this message was for xhost%02d (%s).",
                            //        this_array->n_xhosts, xhost_n, xhost_n_chr);
                            free(xhost_n_chr);
                            if (xhost_n < this_array->n_xhosts)
                            {
                                syslog(LOG_WARNING, "(%s:%s) Fail response to [%s] received. Re-requesting.", this_array->cmc_address, this_array->name, composed_message);
                                queue_push(this_array->outgoing_monitor_msg_queue, this_array->current_monitor_message);
                            }
                            else
                            {
                                syslog(LOG_INFO, "(%s:%s) Fail response to [%s] received. Probably unused x-engine, NOT re-requesting.", this_array->cmc_address, this_array->name, composed_message);
                                message_destroy(this_array->current_monitor_message);
                            }
                        }
                        else
                        {
                            syslog(LOG_WARNING, "(%s:%s) Fail response to '%s' received. Re-requesting.", this_array->cmc_address, this_array->name, composed_message);
                            queue_push(this_array->outgoing_monitor_msg_queue, this_array->current_monitor_message);
                        }
                        this_array->current_monitor_message = NULL;
                    }
                    free(composed_message);

                    if (queue_sizeof(this_array->outgoing_monitor_msg_queue))
                    {
                        array_monitor_queue_pop(this_array);
                        this_array->monitor_state = ARRAY_SEND_FRONT_OF_QUEUE;
                    }
                    else
                    {
                        syslog(LOG_DEBUG, "%s:%s monitor connection going into monitoring state.", this_array->cmc_address, this_array->name);
                        message_destroy(this_array->current_monitor_message);
                        this_array->current_monitor_message = NULL; //doesn't do this in the above function. C problem.
                        this_array->monitor_state = ARRAY_MONITOR;
                    }
                }
                break;
            case '#': // it's a katcp inform
                if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-status") || !strcmp(arg_string_katcl(this_array->monitor_katcl_line, 0) + 1, "sensor-value"))
                {
                    if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, 3), "hostname-functional-mapping"))
                    {
                        if (arg_string_katcl(this_array->monitor_katcl_line, 5) != NULL)
                        {
                            char *sensor_value = strdup(arg_string_katcl(this_array->monitor_katcl_line, 5));
                            syslog(LOG_INFO, "(%s:%s) Received hostname-functional-mapping: %s", this_array->cmc_address, this_array->name, sensor_value);
                            int i;
                            for (i = 0; i < 2*this_array->n_antennas; i++) //hacky. Only works because of fixed-width fields.
                            {
                                if (i*30 + 21 > strlen(sensor_value))
                                        break;
                                char host_type = sensor_value[i*30 + 21];
                                //syslog(LOG_DEBUG, "host type: %c", host_type);

                                char *host_number_str = strndup(sensor_value + (i*30 + 26), 2);
                                size_t host_number = (size_t) atoi(host_number_str);
                                //syslog(LOG_DEBUG, "host number: %lu", host_number);
                                char *host_serial = strndup(sensor_value + (i*30 + 8), 6);
                                //syslog(LOG_DEBUG, "host serial: %s", host_serial);
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
                        else
                        {
                            syslog(LOG_DEBUG, "(%s:%s) Received NULL hostname-functional-mapping!", this_array->cmc_address, this_array->name);
                        }
                        this_array->hostname_functional_mapping_received = 1;
                    }
                    else
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
                            case 'd': // This happens when it's the top-level "device-status" sensor. Expected behaviour.
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
                            //syslog(LOG_DEBUG, "KATCP message from %s:%s for sensor-status %s - null value received.", this_array->cmc_address, this_array->name, arg_string_katcl(this_array->monitor_katcl_line, 3));
                            new_value = strdup("none");
                        }

                        char *new_status;
                        if (arg_string_katcl(this_array->monitor_katcl_line, 4) != NULL) 
                        {
                            new_status = strdup(arg_string_katcl(this_array->monitor_katcl_line, 4));
                        }
                        else
                        {
                            //syslog(LOG_DEBUG, "KATCP message from %s:%s for sensor-status %chost%02ld.%s.%s - null status received.", this_array->cmc_address, this_array->name, team, host_no, tokens[1], tokens[2]);
                            new_status = strdup("none");
                        }

                        switch (n_tokens) {
                            case 1:
                                array_update_top_level_sensor(this_array, tokens[0], new_value, new_status);
                                break;
                            case 2: 
                                ; //Nothing to do here. It's probably a fhost01.device-status or something like that.
                                // will get these when requesting sensor-value for all the sensorz. Just ignore.
                                break;
                            case 3:
                                team_update_sensor(this_array->team_list[team_no], host_no, tokens[1], tokens[2], new_value, new_status);
                                break;
                            case 4:
                                team_update_engine_sensor(this_array->team_list[team_no], host_no, tokens[1], tokens[2], tokens[3], new_value, new_status);
                                break;
                            default:
                                //TODO make this error message a bit more reasonable so that I'd be able to find it if I needed to.
                                syslog(LOG_ERR, "Unexpected number of tokens (%ld) in received KATCP message: %s", n_tokens, arg_string_katcl(this_array->monitor_katcl_line, 3)); 
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
                }
                /*else if (!strcmp(arg_string_katcl(this_array->monitor_katcl_line, -1) + 0, "sensor-list"))
                {
                    //TODO -- it looks as though this functionality is no longer required.
                }*/
                break;
            default:
                ; //This shouldn't ever happen.
        }
    }
}


/**
 * \fn      char *array_html_summary(struct array *this_array, char *cmc_name)
 * \details Generate an HTML summary representation of the array, for when the array is on the main, CMC-list page.
 * \param   this_array A pointer to the array in question.
 * \param   cmc_name A string containing the name of the cmc_server that is the array's parent.
 * \return  A string containing the summary HTML representation of the array.
 */
char *array_html_summary(struct array *this_array, char *cmc_name)
{
    char format[] = "<tr><td><a href=\"%s/%s\">%s</a></td><td>%hu</td><td>%hu</td><td>%lu</td><td>%s</td><td>%s</td>";
    ssize_t needed = snprintf(NULL, 0, format, cmc_name, this_array->name, this_array->name, this_array->control_port, this_array->monitor_port, this_array->n_antennas, this_array->config_file, this_array->instrument_state) + 1;
    //TODO checks
    char *html_summary = malloc((size_t) needed);
    sprintf(html_summary, format, cmc_name, this_array->name, this_array->name, this_array->control_port, this_array->monitor_port, this_array->n_antennas, this_array->config_file, this_array->instrument_state);
    return html_summary;
}


/**
 * \fn      char *array_html_detail(struct array *this_array)
 * \details Generate an HTML detailed representation of the array, for when the array is the focus.
 * \param   this_array A pointer to the array in question.
 * \return  A string with a detailed HTML representation of the array and its children.
 */
char *array_html_detail(struct array *this_array)
{
    char *top_detail = strdup("");
    {
        //collect the top-level sensors first.
        size_t i;
        char *tl_sensors_rep = strdup("");
        for (i = 0; i < this_array->num_top_level_sensors; i++)
        {
            char tl_sensors_format[] = "<button class=\"%s\" style=\"width:300px\">%s</button> ";
            ssize_t needed = (ssize_t) snprintf(NULL, 0, tl_sensors_format, sensor_get_status(this_array->top_level_sensor_list[i]), \
                    sensor_get_name(this_array->top_level_sensor_list[i])) + 1;
            needed += (ssize_t) strlen(tl_sensors_rep);
            tl_sensors_rep = realloc(tl_sensors_rep, (size_t) needed); //TODO check for -1
            sprintf(tl_sensors_rep + strlen(tl_sensors_rep), tl_sensors_format, sensor_get_status(this_array->top_level_sensor_list[i]), \
                    sensor_get_name(this_array->top_level_sensor_list[i]));
        }
        char format[] = "<p align=\"right\">CMC: %s | Array name: %s | Config: %s | %s Last updated: %s (%d seconds ago). <button style=\"width:7%\"><a href=\"/%s/%s/missing-pkts\">missing-pkts</a></button></p>";
        char time_str[20];
        struct tm *last_updated_tm = localtime(&this_array->last_updated);
        strftime(time_str, 20, "%F %T", last_updated_tm);
        ssize_t needed = snprintf(NULL, 0, format, this_array->cmc_address, this_array->name, this_array->config_file, tl_sensors_rep, time_str, (int)(time(0) - this_array->last_updated), this_array->cmc_address, this_array->name) + 1;
        top_detail = realloc(top_detail, (size_t) needed);
        sprintf(top_detail, format, this_array->cmc_address, this_array->name, this_array->config_file, tl_sensors_rep, time_str, (int)(time(0) - this_array->last_updated), this_array->cmc_address, this_array->name);
        free(tl_sensors_rep);
    }
    
    char *array_detail = strdup(""); //must free() later.
    size_t i, j;
    for (i = 0; i < this_array->n_antennas; i++)
    {
        char *row_detail = strdup("");
        for (j = 0; j < this_array->number_of_teams; j++)
        {
            char *host_html_det = team_get_host_html_detail(this_array->team_list[j], i);
            size_t needed =  strlen(row_detail) + strlen(host_html_det) + 1;
            row_detail = realloc(row_detail, needed);
            strcat(row_detail, host_html_det);
            free(host_html_det);
        }
        char format[] = "<tr>%s</tr>\n";
        ssize_t needed = (ssize_t) snprintf(NULL, 0, format, row_detail) + 1;
        needed += (ssize_t) strlen(array_detail);
        //printf("Needed: %zd\n", needed);
        array_detail = realloc(array_detail, (size_t) needed);
        sprintf(array_detail + strlen(array_detail), format, row_detail);
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



/**
 * \fn      char *array_html_missing_pkt_view(struct array *this_array)
 * \details Generate an HTML representation of the array's missing-pkt sensors on the xhosts.
 * \param   this_array A pointer to the array in question.
 * \return  A string with an HTML representation of the array's missing-pkt sensors.
 */
char *array_html_missing_pkt_view(struct array *this_array)
{
    char *top_row_html = strdup("<tr><td> </td>");
    char *second_row_html = strdup("<tr><td> </td>");
    char *array_html = strdup("");
    size_t i, j;
    for (i = 0; i < this_array->n_antennas; i++)
    {
        char top_row_format[] = "<td>f%02d</td>";
        char second_row_format[] = "<td>%s</td>";
        ssize_t needed = (ssize_t) snprintf(NULL, 0, top_row_format, i) + 1; //using vertical axis to build column headings,
        needed += (ssize_t) strlen(top_row_html);                                                                //which should be okay because we assume a square array.
        top_row_html = realloc(top_row_html, (size_t) needed);
        sprintf(top_row_html + strlen(top_row_html), top_row_format, i);

        needed = (ssize_t) snprintf(NULL, 0, second_row_format, team_get_fhost_input_stream(this_array->team_list[0], (size_t) i)) + 1;
        needed += (ssize_t) strlen(second_row_html);
        second_row_html = realloc(second_row_html, (size_t) needed);
        sprintf(second_row_html + strlen(second_row_html), second_row_format, team_get_fhost_input_stream(this_array->team_list[0], (size_t) i));

        char *host_html = strdup("");
        for (j = 0; j < this_array->n_antennas; j++)
        {
            //need to know which sensor to ask for
            char sensor_name_format[] = "fhost%02d-cnt"; 
            ssize_t needed = snprintf(NULL, 0, sensor_name_format, j) + 1;
            char *sensor_name = malloc((size_t) needed);
            sprintf(sensor_name, sensor_name_format, j);

            char html_format[] = "<td class=\"%s\">%s</td>";
            char *sensor_status = array_get_sensor_status(this_array, 'x', i, "missing-pkts", sensor_name);
            char *sensor_value = array_get_sensor_value(this_array, 'x', i, "missing-pkts", sensor_name);
            needed = (ssize_t) snprintf(NULL, 0, html_format, sensor_status, sensor_value) + 1;
            needed += (ssize_t) strlen(host_html);
            host_html = realloc(host_html, (size_t) needed);
            sprintf(host_html + strlen(host_html), html_format, sensor_status, sensor_value);
            
            free(sensor_name);
        }
        //syslog(LOG_DEBUG, "Generated host html: %s", host_html);
        char array_format[] = "<tr><td>x%02d</td>%s</tr>\n";
        needed = (ssize_t) snprintf(NULL, 0, array_format, i, host_html) + 1;
        needed += (ssize_t) strlen(array_html);
        array_html = realloc(array_html, (size_t) needed);
        sprintf(array_html + strlen(array_html), array_format, i, host_html);
        free(host_html);
    }

    char top_row_format[] = "%s</tr>\n";
    top_row_html = realloc(top_row_html, strlen(top_row_html) + strlen(top_row_format) + 1);
    strcat(top_row_html, top_row_format);

    char final_format[] = "<table>%s%s%s</table>";
    ssize_t needed = snprintf(NULL, 0, final_format, top_row_html, second_row_html, array_html) + 1;
    char *final_html = malloc((size_t) needed);
    sprintf(final_html, final_format, top_row_html, second_row_html, array_html);
    free(array_html);
    free(top_row_html);
    free(second_row_html);
    return final_html;
}


/**
 * \fn      struct message *array_control_queue_pop(struct array *this_array)
 * \details Remove the array's current_control_message (if any), and replaces it with one popped from the outgoing_control_msg_queue.
 * \param   this_array A pointer to the array in question.
 * \return  A pointer to the message that was popped from the outgoing_control_msg_queue.
 */
struct message *array_control_queue_pop(struct array *this_array)
{
    if (this_array->current_control_message != NULL)
    {
        message_destroy(this_array->current_control_message);
    }
    this_array->current_control_message = queue_pop(this_array->outgoing_control_msg_queue);
    return this_array->current_control_message;
}


/**
 * \fn      struct message *array_monitor_queue_pop(struct array *this_array)
 * \details Remove the array's current_monitor_message (if any), and replaces it with one popped from the outgoing_monitor_msg_queue.
 * \param   this_array A pointer to the array in question.
 * \return  A pointer to the message that was popped from the outgoing_monitor_msg_queue.
 */
struct message *array_monitor_queue_pop(struct array *this_array)
{
    if (this_array->current_monitor_message != NULL)
    {
        message_destroy(this_array->current_monitor_message);
    }
    this_array->current_monitor_message = queue_pop(this_array->outgoing_monitor_msg_queue);
    return this_array->current_monitor_message;
}

/**
 * \fn      char** array_get_stagnant_sensor_names(struct array *this_array, time_t stagnant_time, size_t *number_of_sensors)
 * \details Get a list of names of the array's sensors (via its child teams) which haven't been updated for a specified amount of time.
 * \param   this_array A pointer to the array.
 * \param   stagnant_time The time in seconds above which sensors should be reported stagnant.
 * \param   number_of_sensors A pointer to an integer so that the function can return the number of sensors in the list.
 * \return  A pointer to an array of strings containing the names of the array's stagnant sensors.
 */
/*char** array_get_stagnant_sensor_names(struct array *this_array, time_t stagnant_time, size_t *number_of_sensors)
{
    *number_of_sensors = 0;
    char **sensor_names = NULL;
    int i;
    for (i = 0; i < this_array->num_top_level_sensors; i++)
    {
        if (time(0) >= sensor_get_last_updated(this_array->top_level_sensor_list[i]) + stagnant_time)
        {
            sensor_names = realloc(sensor_names, sizeof(*sensor_names)*(*number_of_sensors + 1));
            sensor_names[(*number_of_sensors)++] = strdup(sensor_get_name(this_array->top_level_sensor_list[i]));
        }
    }

    for (i = 0; i < this_array->number_of_teams; i++)
    {
        size_t batch_n_sensors;
        char **batch_sensor_names = team_get_stagnant_sensor_names(this_array->team_list[i], stagnant_time, &batch_n_sensors);
        sensor_names = realloc(sensor_names, sizeof(*sensor_names)*(*number_of_sensors + batch_n_sensors));
        int j;
        for (j = 0; j < batch_n_sensors; j++)
        {
            sensor_names[*number_of_sensors + (size_t) j] = strdup(batch_sensor_names[j]);
            free(batch_sensor_names[j]); //can do it here I guess, no need to through another loop?
        }
        free(batch_sensor_names);
        *number_of_sensors += batch_n_sensors;
    }

    if (*number_of_sensors)
        syslog(LOG_DEBUG, "Array %s reported %ld stagnant sensor%s.", this_array->name, *number_of_sensors, *number_of_sensors == 1 ? "" : "s");
    return sensor_names;

} */


