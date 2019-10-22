#include <stdlib.h>
#include <string.h>

#include "team.h"
#include "host.h"


/// A struct for organising host objects of a similar type together.
struct team {
    /// The host type kept in this team, either 'f' or 'x' at this point.
    char host_type;
    /// The number of antennas, "inherited" from the parent array object.
    size_t number_of_antennas;
    /// A list of hosts that are members of the team. The number of hosts is equal to the number of antennas because of MeerKAT's design.
    struct host **host_list;
};


/**
 * \fn      struct team *team_create(char type, size_t number_of_antennas)
 * \details Allocate memory for the team object, create the underlying host objects according to the number of antennas specified.
 * \param   type The type of hosts to create ('f' or 'x').
 * \param   number_of_antennas The number of antennas that the parent array has.
 * \return  A pointer to the newly-created team object.
 */
struct team *team_create(char type, size_t number_of_antennas)
{
    struct team *new_team = malloc(sizeof(*new_team));
    if (new_team != NULL)
    {
        new_team->host_type = type;
        new_team->number_of_antennas = number_of_antennas;
        new_team->host_list = malloc(sizeof(new_team->host_list)*new_team->number_of_antennas);
        int i;
        for (i = 0; i < new_team->number_of_antennas; i++)
        {
            new_team->host_list[i] = host_create(type, i);
        }
    }
    return new_team;
}


/**
 * \fn      void team_destroy(struct team *this_team)
 * \details Free the memory associated with the team object.
 * \param   this_team A pointer to the team in question.
 * \return  void
 */
void team_destroy(struct team *this_team)
{
    if (this_team != NULL)
    {
        unsigned int i;
        for (i = 0; i < this_team->number_of_antennas; i++)
        {
            host_destroy(this_team->host_list[i]);
        }
        free(this_team->host_list);
        free(this_team);
        this_team = NULL;
    }
}


/**
 * \fn      int team_add_device_sensor(struct team *this_team, size_t host_number, char *device_name, char *sensor_name)
 * \details Add a device and sensor to the specified host in this team. If the parent structures already exist, simply add a sensor to it.
 *          If the sensor already exists, nothing will be done.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host which needs to get the device / sensor added.
 * \param   device_name The name of the device to (possibly) create.
 * \param   sensor_name The name of the sensor to create.
 * \return  An integer indicating the outcome of the operation. A positive value (including zero) indicates success,
 *          while a negative value indicates failure.
 */
int team_add_device_sensor(struct team *this_team, size_t host_number, char *device_name, char *sensor_name)
{
    if (this_team != NULL && host_number < this_team->number_of_antennas)
    {
        int r;
        r = host_add_device(this_team->host_list[host_number], device_name);
        if (r < 0)
            return r;
        r = host_add_sensor_to_device(this_team->host_list[host_number], device_name, sensor_name);
        if (r < 0)
            return r;
        return 0;
    }
    return -1;
}


/**
 * \fn      int team_add_engine_device_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char *sensor_name) 
 * \details Add an engine, device and sensor to the specified host in this team. If the parent structures already exist, simply add a sensor to it.
 *          If the sensor already exists, nothing will be done.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host which needs to get the device / sensor added.
 * \param   engine_name The name of the engine to (possibly) create.
 * \param   device_name The name of the device to (possibly) create.
 * \param   sensor_name The name of the sensor to create.
 * \return  An integer indicating the outcome of the operation. A positive value (including zero) indicates success,
 *          while a negative value indicates failure.
 */
int team_add_engine_device_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char *sensor_name)
{
    if (this_team != NULL && host_number < this_team->number_of_antennas)
    {
        int r;
        r = host_add_engine(this_team->host_list[host_number], engine_name);
        if (r < 0)
            return r;
        r = host_add_device_to_engine(this_team->host_list[host_number], engine_name, device_name);
        if (r < 0)
            return r;
        r = host_add_sensor_to_engine_device(this_team->host_list[host_number], engine_name, device_name, sensor_name);
        if (r < 0)
            return r;
        return 0;
    }
    return -1;
}


/**
 * \fn      int team_set_host_serial_no(struct team *this_team, size_t host_number, char *host_serial)
 * \details Set the serial number of one of the hosts in the team.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host whose serial is to be set.
 * \param   host_serial A string containing the serial number of the FPGA host.
 * \return  An integer indicating the outcome of the operation.
 */
int team_set_host_serial_no(struct team *this_team, size_t host_number, char *host_serial)
{
    //TODO this could be a lot more rigorous. Lack of error checking very sloppy...
    // Risk is somewhat low though because it's not user-input, but it comes from the CMC, so the info
    // shouldn't be incorrect and try to write past the end of the list.
    return host_set_serial_no(this_team->host_list[host_number], host_serial);
}


/**
 * \fn      char team_get_type(struct team *this_team)
 * \details Get the type ('f' or 'x') of the hosts grouped together in the team.
 * \param   this_team A pointer to the team in question.
 * \return  A char containing the type of the team.
 */
char team_get_type(struct team *this_team)
{
    if (this_team != NULL)
        return this_team->host_type;
    else
        return -1; /// \retval -1 The team pointer is null and either has been destroyed or not yet properly allocated.
}


/**
 * \fn      int team_set_fhost_input_stream(struct team *this_team, char *input_stream_name, size_t fhost_number)
 * \details Set the input stream of one of the hosts in the team.
 * \param   this_team A pointer to the team in question.
 * \param   input_stream_name A string containing the input stream to which the engines on the host will be subscribing.
 * \param   fhost_number The index of the host whose input stream is to be set.
 * \return  An integer indicating the outcome of the operation.
 */
int team_set_fhost_input_stream(struct team *this_team, char *input_stream_name, size_t fhost_number)
{
    if (this_team != NULL)
    {
        return host_update_input_stream(this_team->host_list[fhost_number], input_stream_name);
    }
    return -1;
}


/**
 * \fn      int team_update_sensor(struct team *this_team, size_t host_number, char *device_name, char*sensor_name, char *new_sensor_value, char *new_sensor_status)
 * \details Update a sensor on one of the hosts in the team.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host in question.
 * \param   device_name A string containing the name of the device in question.
 * \param   sensor_name A string containing the name of the sensor to be updated.
 * \param   new_sensor_value A string containing the new sensor value.
 * \param   new_sensor_status A string containing the new status of the sensor.
 * \return  An integer indicating the outcome of the operation.
 */
int team_update_sensor(struct team *this_team, size_t host_number, char *device_name, char*sensor_name, char *new_sensor_value, char *new_sensor_status)
{
    if (this_team != NULL)
    {
        if (host_number >= this_team->number_of_antennas)
            return -2;
        return host_update_sensor(this_team->host_list[host_number], device_name, sensor_name, new_sensor_value, new_sensor_status);
    }
    return -1;
}


/**
 * \fn      int team_update_engine_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char*sensor_name, char *new_sensor_value, char *new_sensor_status)
 * \details Update a sensor (underneath an engine) on one of the hosts in the team.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host in question.
 * \param   engine_name The name of the engine in question.
 * \param   device_name A string containing the name of the device in question.
 * \param   sensor_name A string containing the name of the sensor to be updated.
 * \param   new_sensor_value A string containing the new sensor value.
 * \param   new_sensor_status A string containing the new status of the sensor.
 * \return  An integer indicating the outcome of the operation.
 */
int team_update_engine_sensor(struct team *this_team, size_t host_number, char *engine_name, char *device_name, char *sensor_name, char *new_sensor_value, char *new_sensor_status)
{
    if (this_team != NULL)
    {
        if (host_number >= this_team->number_of_antennas)
            return -2;
        return host_update_engine_sensor(this_team->host_list[host_number], engine_name, device_name, sensor_name, new_sensor_value, new_sensor_status);
    }
    return -1; 
}


/**
 * \fn      char *team_get_sensor_value(struct team *this_team, size_t host_number, char *device_name, char *sensor_name)
 * \details Get the value for the sensor specified.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host in question.
 * \param   device_name A string containing the name of the device in question.
 * \param   sensor_name A string containing the name of the sensor to be updated.
 * \return  A string containing the value of the queried sensor. This string is not newly allocated and must not be freed.
 */
char *team_get_sensor_value(struct team *this_team, size_t host_number, char *device_name, char *sensor_name)
{
    if  (this_team != NULL)
    {
        if (host_number < this_team->number_of_antennas)
            return host_get_sensor_value(this_team->host_list[host_number], device_name, sensor_name);
    }
    return NULL;
}


/**
 * \fn      char *team_get_sensor_status(struct team *this_team, size_t host_number, char *device_name, char *sensor_name)
 * \details Get the status for the sensor specified.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host in question.
 * \param   device_name A string containing the name of the device in question.
 * \param   sensor_name A string containing the name of the sensor to be updated.
 * \return  A string containing the status of the queried sensor. This string is not newly allocated and must not be freed.
 */
char *team_get_sensor_status(struct team *this_team, size_t host_number, char *device_name, char *sensor_name)
{
    if  (this_team != NULL)
    {
        if (host_number < this_team->number_of_antennas)
            return host_get_sensor_status(this_team->host_list[host_number], device_name, sensor_name);
    }
    return NULL;

}


/**
 * \fn      char *team_get_host_html_detail(struct team *this_team, size_t host_number)
 * \details Get an HTML representation of the host at the specified index.
 * \param   this_team A pointer to the team in question.
 * \param   host_number The index of the host in question.
 * \return  If the host exists, a newly-allocated string containing the HTML representation of the host. Otherwise, NULL.
 */
char *team_get_host_html_detail(struct team *this_team, size_t host_number)
{
    if (host_number < this_team->number_of_antennas)
    {
        return host_html_detail(this_team->host_list[host_number]);
    }
    else
        return NULL;
}
