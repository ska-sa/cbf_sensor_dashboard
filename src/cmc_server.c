#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <katcp.h>
#include <katcl.h>
#include <netc.h>
#include <string.h>
#include <stdint.h>
#include <syslog.h>
#include <ctype.h>

#include "cmc_server.h"
#include "queue.h"
#include "message.h"
#include "utils.h"
#include "array.h"

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

enum cmc_state {
    CMC_WAIT_CONNECT,
    CMC_SEND_FRONT_OF_QUEUE,
    CMC_WAIT_RESPONSE,
    CMC_MONITOR,
    CMC_DISCONNECTED,
};


/// A struct to manage the connection to a CMC server.
struct cmc_server {
    /// The port on which the CMC server is listening for KATCP connections. Usually 7147.
    uint16_t katcp_port;
    /// The IP address or (resolvable) hostname of the CMC server.
    char *address;
    /// The file descriptor for the connection.
    int katcp_socket_fd;
    /// katcl_line object to handle interpreting the incoming katcp messages.
    struct katcl_line *katcl_line;
    /// The current state of the connection state-machine.
    enum cmc_state state;
    /// The queue storing messages to be sent. A queue is needed because the CMC server wants you to wait for the ok response before sending another message.
    struct queue *outgoing_msg_queue;
    /// The most recent message that's been sent, so that we can match it against the response received, to know whether it's failed or not.
    struct message *current_message;
    /// The list of arrays that the CMC server is currently managing.
    struct array **array_list;
    /// The number of arrays in the list.
    size_t no_of_arrays;
};


/**
 * \fn      struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port)
 * \details Allocate memory for a cmc_server object and initialise its members so that it gets ready to start communicating with the CMC server.
 *          The object is created with a hard-coded list of initial messages to send: "?log-local off", "?client-config info-all", and "?array-list".
 * \param   address A string containing the IP address or (resolvable) hostname of the CMC server.
 * \param   katcp_port The TCP port on which the CMC server is listening for KATCP connections.
 * \returns A pointer to the newly-allocated cmc_server object.
 */
struct cmc_server *cmc_server_create(char *address, uint16_t katcp_port)
{
    struct cmc_server *new_cmc_server = malloc(sizeof(*new_cmc_server));
    new_cmc_server->address = strdup(address);
    new_cmc_server->katcp_port = katcp_port;
    new_cmc_server->katcp_socket_fd = net_connect(address, katcp_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS | NETC_ASYNC | NETC_TCP_KEEP_ALIVE );
    new_cmc_server->katcl_line = NULL;
    new_cmc_server->outgoing_msg_queue = queue_create();

    new_cmc_server->array_list = NULL;
    new_cmc_server->no_of_arrays = 0;

    /*This bit is hardcoded for the time being. Perhaps a better way would be to include it in a config
     * file like the sensors to which we'll be subscribing. */
    struct message *new_message = message_create('?');
    message_add_word(new_message, "log-local");
    message_add_word(new_message, "off");
    queue_push(new_cmc_server->outgoing_msg_queue, new_message);

    new_message = message_create('?');
    message_add_word(new_message, "client-config");
    message_add_word(new_message, "info-all");
    queue_push(new_cmc_server->outgoing_msg_queue, new_message);

    new_message = message_create('?');
    message_add_word(new_message, "array-list");
    queue_push(new_cmc_server->outgoing_msg_queue, new_message);

    new_cmc_server->current_message = NULL;
    cmc_server_queue_pop(new_cmc_server);
    new_cmc_server->state = CMC_WAIT_CONNECT;
    return new_cmc_server;
}


/**
 * \fn      void cmc_server_destroy(struct cmc_server *this_cmc_server)
 * \details Free the memory allocated to the cmc_server object and its children.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  void
 */
void cmc_server_destroy(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server != NULL)
    {
        if (this_cmc_server->katcl_line != NULL) //because it might not have actually been connected.
            destroy_katcl(this_cmc_server->katcl_line, 1);
        close(this_cmc_server->katcp_socket_fd);
        queue_destroy(this_cmc_server->outgoing_msg_queue);
        message_destroy(this_cmc_server->current_message);
        size_t i;
        for (i = 0; i < this_cmc_server->no_of_arrays; i++)
        {
            array_destroy(this_cmc_server->array_list[i]);
        }
        free(this_cmc_server->array_list);
        free(this_cmc_server->address);
        free(this_cmc_server);
    }
}


/**
 * \fn      void cmc_server_try_reconnect(struct cmc_server *this_cmc_server)
 * \details If the cmc_server's state indicates that it is disconnected, try to reconnect.
 *          Honestly, I'm not entirely sure that this actually works properly.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  void
 */
void cmc_server_try_reconnect(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->state == CMC_DISCONNECTED)
    {
        close(this_cmc_server->katcp_socket_fd);
        //TODO destroy all the arrays underneath as well?
        this_cmc_server->katcp_socket_fd = net_connect(this_cmc_server->address, this_cmc_server->katcp_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS | NETC_ASYNC | NETC_TCP_KEEP_ALIVE);
        this_cmc_server->state = CMC_WAIT_CONNECT;
    }
}


/**
 * \fn      void cmc_server_poll_array_list(struct cmc_server *this_cmc_server)
 * \details This function adds an "?array-list" message to the cmc_server's message queue and sets it up to send (if it's not busy with other things).
 *          Additionally, each array on the cmc_server's list is marked as "suspect", so that arrays which are not named in the #array-list informs received
 *          in response to this query can be "garbage-collected".
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  void
 */
void cmc_server_poll_array_list(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->state != CMC_DISCONNECTED && this_cmc_server->state != CMC_WAIT_CONNECT) //no sense if we're not connected yet.
    {
        //Mark the arrays as potentially missing.
        size_t i;
        for (i = 0; i < this_cmc_server->no_of_arrays; i++)
        {
            array_mark_suspect(this_cmc_server->array_list[i]);
        }
        struct message *new_message = message_create('?');
        message_add_word(new_message, "array-list");
        queue_push(this_cmc_server->outgoing_msg_queue, new_message);
        char *message_exists = message_compose(this_cmc_server->current_message);
       	// Don't just check for null, because a message might exists with zero words in it somehow. If it composes to a usable string, then it's legit.
        if (!message_exists)
            cmc_server_queue_pop(this_cmc_server);
        free(message_exists);
        //syslog(LOG_DEBUG, "%s:%hu pushed an array-list poll onto its message queue.", this_cmc_server->address, this_cmc_server->katcp_port);
        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
    }
}


/**
 * \fn      struct message *cmc_server_queue_pop(struct cmc_server *this_cmc_server)
 * \details Remove the cmc_server's current_message (if any), and replaces it with one popped from the outgoing_msg_queue.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  A pointer to the message that was popped from the outgoing_msg_queue.
 */
struct message *cmc_server_queue_pop(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->current_message != NULL)
    {
        message_destroy(this_cmc_server->current_message);
    }
    this_cmc_server->current_message = queue_pop(this_cmc_server->outgoing_msg_queue);
    return this_cmc_server->current_message;
}


/**
 * \fn      char *cmc_server_get_name(struct cmc_server *this_cmc_server)
 * \details Get the address of the cmc_server object. Typically this has been in the form of its (resolvable) hostname, so the address doubles
 *          as a name quite readily.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  A string containing the address of the cmc_server. This is not newly allocated and must not be freed.
 */
char *cmc_server_get_name(struct cmc_server *this_cmc_server)
{
    return this_cmc_server->address;
}


/**
 * \fn      void cmc_server_set_fds(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr, int *nfds)
 * \details This function sets both read and write file desciptors according to the state that the cmc_server's state machine is in.
 *          the rd file descriptor will almost always be set if the connection is active, with the wr file descriptor set only if there
 *          is a message waiting to be sent.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \param   rd A pointer to the fd_set indicating ready to read.
 * \param   wr A pointer to the fd_set indicating ready to write.
 * \param   nfds A pointer to an integer indicating the number of file descriptors in the above sets.
 * \return  void
 */
void cmc_server_set_fds(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr, int *nfds)
{
    /* //I forget why this code is here.
    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(this_cmc_server->katcp_socket_fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0)
    {
        this_cmc_server->state = CMC_DISCONNECTED;
    }
    */
    switch (this_cmc_server->state) {
        case CMC_WAIT_CONNECT:
            syslog(LOG_NOTICE, "CMC server %s:%hu still not connected...\n", this_cmc_server->address, this_cmc_server->katcp_port);
            FD_SET(this_cmc_server->katcp_socket_fd, wr); // If we're still waiting for the connect() to happen, then it'll appear on the writeable FDs.
            *nfds = max(*nfds, this_cmc_server->katcp_socket_fd);
            break;
        case CMC_DISCONNECTED:
            break; //Nothing to do here.
        default:
            FD_SET(this_cmc_server->katcp_socket_fd, rd);
            if (flushing_katcl(this_cmc_server->katcl_line))
            {
                FD_SET(this_cmc_server->katcp_socket_fd, wr);
            }
            *nfds = max(*nfds, this_cmc_server->katcp_socket_fd);
            
            //now for the individual arrays.
            size_t i;
            for (i = 0; i < this_cmc_server->no_of_arrays; i++)
            {
                if (array_functional(this_cmc_server->array_list[i]) >= 0)
                    array_set_fds(this_cmc_server->array_list[i], rd, wr, nfds);
                //TODO some other way to deal with this.
            }
    }
}


/**
 * \fn      void cmc_server_setup_katcp_writes(struct cmc_server *this_cmc_server)
 * \details If there is a message waiting to be sent, this function will insert it into the katcl_line, word for word, until it's finished.
 *          On the next select() loop, the katcl_line will then report that it's ready to write a fully-formed message to the file descriptor.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  void
 */
void cmc_server_setup_katcp_writes(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->current_message)
    {
        if (this_cmc_server->state == CMC_SEND_FRONT_OF_QUEUE)
        {
            int n = message_get_number_of_words(this_cmc_server->current_message);
            if (n > 0)
            {
                char *composed_message = message_compose(this_cmc_server->current_message);
                syslog(LOG_DEBUG, "%s is sending a message: %s", this_cmc_server->address, composed_message);
                free(composed_message);
                composed_message = NULL;

                char *first_word = malloc(strlen(message_see_word(this_cmc_server->current_message, 0)) + 2);
                sprintf(first_word, "%c%s", message_get_type(this_cmc_server->current_message), message_see_word(this_cmc_server->current_message, 0));
                if (message_get_number_of_words(this_cmc_server->current_message) == 1)
                {
                    append_string_katcl(this_cmc_server->katcl_line, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, first_word);
                }
                else
                {
                    append_string_katcl(this_cmc_server->katcl_line, KATCP_FLAG_FIRST, first_word);
                    size_t j;
                    for (j = 1; j < n - 1; j++)
                    {
                        append_string_katcl(this_cmc_server->katcl_line, 0, message_see_word(this_cmc_server->current_message, j));
                    }
                    append_string_katcl(this_cmc_server->katcl_line, KATCP_FLAG_LAST, message_see_word(this_cmc_server->current_message, (size_t) n - 1));
                }

                syslog(LOG_DEBUG, "%s message sent successfully.", first_word);
                free(first_word);
                this_cmc_server->state = CMC_WAIT_RESPONSE;
            }
            /*
            else
            {
                syslog(LOG_WARNING, "A message on %s:%hu's queue had 0 words in it. Cannot send.", this_cmc_server->address, this_cmc_server->katcp_port);
                //TODO push through the queue if there's an error.
            }
            */
        }
    }

    size_t i;
    for (i=0; i < this_cmc_server->no_of_arrays; i++)
    {
        array_setup_katcp_writes(this_cmc_server->array_list[i]);
    }
}


/**
 * \fn      void cmc_server_socket_read_write(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr)
 * \details Depending on the state that the cmc_server's state machine is in, send all transmissions which are ready, and read
 *          incoming transmissions, storing them in the katcl_line for processing once a fully-formed message is received.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \param   rd A pointer to the fd_set indicating ready to read.
 * \param   wr A pointer to the fd_set indicating ready to write.
 * \return  void
 */
void cmc_server_socket_read_write(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr)
{
    switch (this_cmc_server->state) {
        case CMC_WAIT_CONNECT:
            if (FD_ISSET(this_cmc_server->katcp_socket_fd, wr))
            {
                syslog(LOG_DEBUG, "%s:%hu file descriptor writeable.", this_cmc_server->address, this_cmc_server->katcp_port);
                int so_error;
                socklen_t socklen = sizeof(so_error);
                getsockopt(this_cmc_server->katcp_socket_fd, SOL_SOCKET, SO_ERROR, &so_error, &socklen);
                if (so_error == 0)
                {
                    //Connection is a success
                    syslog(LOG_INFO, "%s:%hu connected.", this_cmc_server->address, this_cmc_server->katcp_port);
                    this_cmc_server->katcl_line = create_katcl(this_cmc_server->katcp_socket_fd);
                    this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                }
                else
                {
                    //Connection failed for whatever reason.
                    syslog(LOG_ERR, "Connection to %s%hu failed: %s", this_cmc_server->address, this_cmc_server->katcp_port, strerror(so_error));
                    this_cmc_server->state = CMC_DISCONNECTED;
                }
            }
            break;

        case CMC_DISCONNECTED:
            ; //Do nothing.
            break;

        default: ; //for some reason a label (default) can only be followed by a statement, and my "int r;" is a declaration, not a statement.
            int r;
            size_t i;
            if (FD_ISSET(this_cmc_server->katcp_socket_fd, rd))
            {
                r = read_katcl(this_cmc_server->katcl_line);
                if (r)
                {
                    fprintf(stderr, "read from %s:%hu on fd %d failed\n", this_cmc_server->address, this_cmc_server->katcp_port, this_cmc_server->katcp_socket_fd);
                    /*TODO some kind of error checking, what to do if the CMC doesn't connect.*/
                    this_cmc_server->state = CMC_DISCONNECTED;
                }
            }
            
            if (FD_ISSET(this_cmc_server->katcp_socket_fd, wr))
            {
                r = write_katcl(this_cmc_server->katcl_line);
                if (r < 0)
                {
                    /*TODO some other kind of error checking.*/
                    this_cmc_server->state = CMC_DISCONNECTED;
                }
            }

            for (i=0; i < this_cmc_server->no_of_arrays; i++)
            {
                array_socket_read_write(this_cmc_server->array_list[i], rd, wr);
            }
    }
}


/**
 * \fn      static int cmp_array_by_name(const void *p1, const void *p2)
 * \details Compare two arrays by their name, for use with qsort function. Includes casting the pointers appropriately and getting their names.
 */
static int cmp_array_by_name(const void *p1, const void *p2)
{
    /* This one was tricky.
     * qsort passes the compare function, a pointer to the element of the array that it's sorting.
     * In this case, it's another pointer. So p1 is a pointer to a pointer to a struct array.
     *
     * So we create a1 and a2 which are struct array *, like the array_get_name() function expects.
     * We then need to cast p1 and p2 to struct array **, and dereference once, to get struct array *.
     */
    struct array *a1 = *(struct array **) p1;
    struct array *a2 = *(struct array **) p2;

    char *s1 = array_get_name(a1);
    char *s2 = array_get_name(a2);

    return strcmp(s1, s2);
}


/**
 * \fn      static int cmc_server_add_array(struct cmc_server *this_cmc_server, char *array_name, uint16_t control_port, uint16_t monitor_port, size_t number_of_antennas)
 * \details Add a newly-created array to the cmc_server's array_list.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \param   array_name A string containing the name of the new array to be created.
 * \param   control_port The TCP port on which the corr2_servlet of the new array is listening.
 * \param   monitor_port The TCP port on which the corr2_sensor_servlet of the new array is listening. This is where we'll get most of our useful information.
 * \param   number_of_antennas The number of antennas of the new array (or more correctly, the size of the new correlator).
 * \return  An integer indicating the outcome of the operation.
 */
static int cmc_server_add_array(struct cmc_server *this_cmc_server, char *array_name, uint16_t control_port, uint16_t monitor_port, size_t number_of_antennas)
{
    size_t i;
    for (i = 0; i < this_cmc_server->no_of_arrays; i++)
    {
        if (!strcmp(array_name, array_get_name(this_cmc_server->array_list[i])))
        {
            //Might want to think about comparing the other stuff as well, just in case for some reason the
            //original array got destroyed and another different one but called the same name snuck in.
            array_mark_fine(this_cmc_server->array_list[i]);
            return (int) i;
        }
    }

    /* If not, allocate space for it on the end. */
    struct array **temp = realloc(this_cmc_server->array_list, sizeof(*(this_cmc_server->array_list))*(this_cmc_server->no_of_arrays + 1));
    if (temp == NULL)
    {
        syslog(LOG_ERR, "Unable to realloc memory to add array \"%s\" to %s:%hu.", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
        return -1;
    }
    this_cmc_server->array_list = temp;
    this_cmc_server->array_list[this_cmc_server->no_of_arrays] = array_create(array_name, this_cmc_server->address, control_port, monitor_port, number_of_antennas);
    if (this_cmc_server->array_list[this_cmc_server->no_of_arrays] == NULL)
    {
        syslog(LOG_ERR, "Unable to create array \"%s\" on %s:%hu.", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
        return -1;
    }
    syslog(LOG_INFO, "Added array \"%s\" to %s:%hu.", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
    this_cmc_server->no_of_arrays++;

    qsort(this_cmc_server->array_list, this_cmc_server->no_of_arrays, sizeof(struct array *), cmp_array_by_name);
    return 0;
}


/**
 * \fn      void cmc_server_handle_received_katcl_lines(struct cmc_server *this_cmc_server)
 * \details This function checks whether the katcl_line has any messages ready, and then processes the message, in accordance with the logic of the
 *          built-in state machine.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  void
 */
void cmc_server_handle_received_katcl_lines(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->state == CMC_WAIT_CONNECT || this_cmc_server->state == CMC_DISCONNECTED)
    {
        return; //nothing to do here.
    }

    //let's try doing the array stuff before the cmc stuff
    size_t i;
    for (i=0; i < this_cmc_server->no_of_arrays; i++)
    {
        array_handle_received_katcl_lines(this_cmc_server->array_list[i]);
    }
    while (have_katcl(this_cmc_server->katcl_line) > 0)
    {
        char received_message_type = arg_string_katcl(this_cmc_server->katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, message_see_word(this_cmc_server->current_message, 0)))
                {
                    if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 1), "ok"))
                    {
                        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                        if (queue_sizeof(this_cmc_server->outgoing_msg_queue))
                        {
                            cmc_server_queue_pop(this_cmc_server);
                        }
                        else
                        {
                            syslog(LOG_DEBUG, "%s:%hu going into monitoring state.", this_cmc_server->address, this_cmc_server->katcp_port);
                            message_destroy(this_cmc_server->current_message);
                            this_cmc_server->current_message = NULL; //doesn't do this in the above function. C problem.
                            this_cmc_server->state = CMC_MONITOR;
                        }
                    }
                    else 
                    {
                        syslog(LOG_WARNING, "Received %s %s. Retrying the request...",\
                                message_see_word(this_cmc_server->current_message, 0), arg_string_katcl(this_cmc_server->katcl_line, 1));
                        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                    }
                    //If the "!array-list ok" message is received, we need to prune the arrays which aren't active anymore. 
                    if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, "array-list"))
                    {
                        size_t i;
                        for (i = 0; i < this_cmc_server->no_of_arrays; i++)
                        {
                            if (array_check_suspect(this_cmc_server->array_list[i]))
                            {
                                syslog(LOG_INFO, "%s:%hu destroying array %s.\n", this_cmc_server->address, this_cmc_server->katcp_port, array_get_name(this_cmc_server->array_list[i]));
                                array_destroy(this_cmc_server->array_list[i]);
                                memmove(&this_cmc_server->array_list[i], &this_cmc_server->array_list[i+1], sizeof(*(this_cmc_server->array_list))*(this_cmc_server->no_of_arrays - i - 1));
                                this_cmc_server->array_list = realloc(this_cmc_server->array_list, sizeof(*(this_cmc_server->array_list))*(this_cmc_server->no_of_arrays - 1));
                                //TODO should probably do the sanitary thing here and use a temp variable. Lazy right now.
                                this_cmc_server->no_of_arrays--;
                                i--;
                            }
                        }
                    }
                }
                break;
            case '#': // it's a katcp inform
                /*TODO handle the array-list stuff. code should be easy enough to copy from previous attempt.*/
                if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, "array-list"))
                {
                    char* array_name = arg_string_katcl(this_cmc_server->katcl_line, 1);
                    uint16_t control_port = (uint16_t) atoi(strtok(arg_string_katcl(this_cmc_server->katcl_line, 2), ",")); 
                    uint16_t monitor_port = (uint16_t) atoi(strtok(NULL, ","));
                    /* will leave this here while I can't think of anything to do with the multicast groups.
                    int j = 3;
                    char *multicast_groups = malloc(1);
                    multicast_groups[0] = '\0';
                    char *buffer;
                    do {
                        buffer = arg_string_katcl(this_cmc_server->katcl_line, j);
                        if (buffer)
                        {
                            multicast_groups = realloc(multicast_groups, strlen(multicast_groups) + strlen(buffer) + 2);
                            strcat(multicast_groups, " ");
                            strcat(multicast_groups, buffer);
                        }
                        j++;
                    } while (buffer);
                    free(multicast_groups);
                    multicast_groups = NULL; */

                    //count the number of multicast groups, number of antennas is this /2.
                    size_t j = 3;
                    char * buffer;
                    do {
                        buffer = arg_string_katcl(this_cmc_server->katcl_line, (uint32_t) j);
                        j++;
                    } while (buffer);
                    size_t number_of_antennas = (j - 4)/2; //to take into account the ++ which will have followed the null buffer

                    cmc_server_add_array(this_cmc_server, array_name, control_port, monitor_port, number_of_antennas);
                    //TODO check if return is proper.
                }
                /* //going to ignore these for the time being.
                else if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, "group-created"))
                {
                    char *temp = arg_string_katcl(this_cmc_server->katcl_line, 1);
                    char *new_array_name = malloc(strlen(temp)); //This is bigger than it strictly needs to be, but I'm lazy at the moment.
                    sprintf(new_array_name, "%s", strtok(temp, "."));
                    temp = strtok(NULL, ".");
                    if (!strcmp(temp, "monitor")) //i.e. ignore otherwise. Not interested in the "control" group.
                    {
                        verbose_message(INFO, "Noticed new array: %s\n", new_array_name);
                        //The #group-created message doesn't tell us the port that the new array is on,
                        //or the number of antennas. So we'll request the array-list again. Exisitng arrays
                        //will not be modified (checks for them by name as in above function) but the new one
                        //will be added to the end of the list.
                        struct message *new_message = message_create('?');
                        message_add_word(new_message, "array-list");
                        int r = queue_push(this_cmc_server->outgoing_msg_queue, new_message);
                        if (r<0)
                            verbose_message(ERROR, "Couldn't push message onto the queue.\n");
                        if (!this_cmc_server->current_message)
                            cmc_server_queue_pop(this_cmc_server);
                        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                    }
                    free(new_array_name);
                }
                else if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, "group-destroyed"))
                {
                    char *name_of_removed_array = strtok(arg_string_katcl(this_cmc_server->katcl_line, 1), ".");
                    if (!strcmp(strtok(NULL, "."), "monitor")) //again, not concerned about the control one.
                    {
                        size_t j;
                        for (j = 0; j < this_cmc_server->no_of_arrays; j++)
                        {
                            if (!strcmp(name_of_removed_array, array_get_name(this_cmc_server->array_list[j])))
                                break;
                        }
                        if (j == this_cmc_server->no_of_arrays)
                        {
                            verbose_message(WARNING, "%s:%hu has indicated that array %s is being destroyed, but we weren't aware of it.\n", this_cmc_server->address, this_cmc_server->katcp_port, name_of_removed_array);
                        }
                        else
                        {
                            verbose_message(INFO, "%s:%hu destroying array %s.\n", this_cmc_server->address, this_cmc_server->katcp_port, name_of_removed_array);
                            array_destroy(this_cmc_server->array_list[j]);
                            memmove(&this_cmc_server->array_list[j], &this_cmc_server->array_list[j+1], sizeof(*(this_cmc_server->array_list))*(this_cmc_server->no_of_arrays - j - 1));
                            this_cmc_server->array_list = realloc(this_cmc_server->array_list, sizeof(*(this_cmc_server->array_list))*(this_cmc_server->no_of_arrays - 1));
                            //TODO should probably do the sanitary thing here and use a temp variable. Lazy right now.
                            this_cmc_server->no_of_arrays--;
                        }
                    }
                }*/
                break;
            default:
                syslog(LOG_NOTICE, "Unexpected KATCP message received, starting with %c", received_message_type);
        }
    }
}


/**
 * \fn      char *cmc_server_html_representation(struct cmc_server *this_cmc_server)
 * \details This funcion generates an HTML representation of the CMC server's current array-list, showing a brief description of each array in a table.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \return  A newly-allocated string containing the cmc_server's HTML representation.
 */
char *cmc_server_html_representation(struct cmc_server *this_cmc_server)
{
    char *cmc_html_rep;
    switch (this_cmc_server->state) {
        case CMC_WAIT_CONNECT:
            {
                char format[] = "<h1>%s</h1>\n<p>Connecting to CMC server...</p>\n";
                ssize_t needed = snprintf(NULL, 0, format, this_cmc_server->address) + 1;
                cmc_html_rep = malloc((size_t) needed);
                sprintf(cmc_html_rep, format, this_cmc_server->address);
                return cmc_html_rep;
            }
            break;
         case CMC_DISCONNECTED:
            {
                char format[] = "<h1>%s</h1>\n<p>Could not connect to CMC server...</p>\n";
                ssize_t needed = snprintf(NULL, 0, format, this_cmc_server->address) + 1;
                cmc_html_rep = malloc((size_t) needed);
                sprintf(cmc_html_rep, format, this_cmc_server->address);
                return cmc_html_rep;
            }
            break;
        default:
            if (this_cmc_server->no_of_arrays < 1)
            {
                char format[] = "<h1>%s</h1>\n<p>No arrays currently running.</p>\n";
                ssize_t needed = snprintf(NULL, 0, format, this_cmc_server->address) + 1;
                cmc_html_rep = malloc((size_t) needed);
                sprintf(cmc_html_rep, format, this_cmc_server->address);
                return cmc_html_rep;
            }

            {   //putting this in its own block so that I can reuse the names "format" and "needed" later.
                //might not be ready since this is followed by a for-loop, but anyway.
                char format[] = "<h1>%s</h1>\n<table class=\"cmctable\">\n<tr><th>Array Name</th><th>Control Port</th><th>Monitor Port</th><th>N_Antennas</th><th>Config File</th><th>Instrument State</th></tr>";
                ssize_t needed = snprintf(NULL, 0, format, this_cmc_server->address) + 1;
                //TODO checks
                cmc_html_rep = malloc((size_t) needed);
                sprintf(cmc_html_rep, format, this_cmc_server->address);
            }
            
            size_t i;
            for (i = 0; i < this_cmc_server->no_of_arrays; i++)
            {
                char format[] = "%s%s\n";
                char *array_html_rep = array_html_summary(this_cmc_server->array_list[i], this_cmc_server->address);
                ssize_t needed = snprintf(NULL, 0, format, cmc_html_rep, array_html_rep) + 1;
                //TODO checks
                cmc_html_rep = realloc(cmc_html_rep, (size_t) needed); //naughty naughty, no temp variable.
                sprintf(cmc_html_rep, format, cmc_html_rep, array_html_rep);
                free(array_html_rep);
            }

            {
                char *format = "%s</table>\n";
                ssize_t needed = snprintf(NULL, 0, format, cmc_html_rep) + 1;
                //TODO checks
                cmc_html_rep = realloc(cmc_html_rep, (size_t) needed);
                sprintf(cmc_html_rep, format, cmc_html_rep);
            }
    }
    return cmc_html_rep;
}


/**
 * \fn      int cmc_server_check_for_array(struct cmc_server *this_cmc_server, char *array_name)
 * \details This function checks whether a given array is actually on its list.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \param   array_name A string containing the name of the array to be looked for. If the string is actually a number,
 *          and the number is lower than or equal to the number of arrays currently present, then the matching array is also valid.
 * \return  A positive integer corresponding to the array's position in the array_list if found, minus one if the array was not found.
 */
int cmc_server_check_for_array(struct cmc_server *this_cmc_server, char *array_name)
{
    //Check whether array_name is a number.
    size_t i;
    for (i = 0; i < strlen(array_name); i++)
    {
        if (!isdigit(array_name[i]))
            break;
    }
    if (i == strlen(array_name)) //i.e. we haven't broken out of the loop, array_name is actually a number.
    {
        int r = atoi(array_name);
        if ( 0 < r && r <= this_cmc_server->no_of_arrays)
            return r - 1; //minus one because the actual list is zero-indexed... an important thing not to forget!
    } //if not, perhaps someone stupidly named an array "1234" or some such, in that case this should go through the normal procedure.

    for (i = 0; i < this_cmc_server->no_of_arrays; i++)
    {
        if (!strcmp(array_name, array_get_name(this_cmc_server->array_list[i])))
        {
            return (int) i;
        }
    }
    return -1;
}


/**
 * \fn      struct array *cmc_server_get_array(struct cmc_server *this_cmc_server, size_t array_number)
 * \details Get a pointer to the array on the cmc_server's array_list.
 * \param   this_cmc_server A pointer to the cmc_server in question.
 * \param   array_number The index of the array desired.
 * \return  A pointer to the requested array in the array_list, NULL if the index given is past the end of the list.
 */
struct array *cmc_server_get_array(struct cmc_server *this_cmc_server, size_t array_number)
{
    if (array_number < this_cmc_server->no_of_arrays)
        return this_cmc_server->array_list[array_number];
    else
        return NULL;
}
