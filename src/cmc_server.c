#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <katcp.h>
#include <katcl.h>
#include <netc.h>
#include <string.h>
#include <stdint.h>

#include "verbose.h"

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


struct cmc_server {
    uint16_t katcp_port;
    char *address;
    int katcp_socket_fd;
    struct katcl_line *katcl_line;
    enum cmc_state state;
    struct queue *outgoing_msg_queue;
    struct message *current_message;
    struct array **array_list;
    size_t no_of_arrays;
};


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
    //new_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
    new_cmc_server->state = CMC_WAIT_CONNECT;
    return new_cmc_server;
}


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


struct message *cmc_server_queue_pop(struct cmc_server *this_cmc_server)
{
    if (this_cmc_server->current_message != NULL)
    {
        message_destroy(this_cmc_server->current_message);
    }
    this_cmc_server->current_message = queue_pop(this_cmc_server->outgoing_msg_queue);
    return this_cmc_server->current_message;
}


char *cmc_server_get_name(struct cmc_server *this_cmc_server)
{
    return this_cmc_server->address;
}


void cmc_server_set_fds(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr, int *nfds)
{
    //verbose_message(BORING, "Setting CMC server (%s:%hu) FDs. Current state: %d.\n", this_cmc_server->address, this_cmc_server->katcp_port, this_cmc_server->state);
    /*
    int so_error;
    socklen_t len = sizeof(so_error);
    getsockopt(this_cmc_server->katcp_socket_fd, SOL_SOCKET, SO_ERROR, &so_error, &len);
    if (so_error != 0)
    {
        verbose_message(ERROR, "Socket error on %s%hu: %s\n", this_cmc_server->address, this_cmc_server->katcp_port, strerror(so_error));
        this_cmc_server->state = CMC_DISCONNECTED;
    }
    */
    switch (this_cmc_server->state) {
        case CMC_WAIT_CONNECT:
            verbose_message(DEBUG, "CMC server %s:%hu still not connected...\n", this_cmc_server->address, this_cmc_server->katcp_port);
            FD_SET(this_cmc_server->katcp_socket_fd, wr); // If we're still waiting for the connect() to happen, then it'll appear on the writeable FDs.
            *nfds = max(*nfds, this_cmc_server->katcp_socket_fd);
            break;
        case CMC_DISCONNECTED:
            break; //Nothing to do here.
        default:
            FD_SET(this_cmc_server->katcp_socket_fd, rd);
            if (flushing_katcl(this_cmc_server->katcl_line))
            {
                verbose_message(BORING, "flushing_katcl() returned true, %s:%hu has a katcp command to send.\n", this_cmc_server->address, this_cmc_server->katcp_port);
                FD_SET(this_cmc_server->katcp_socket_fd, wr);
            }
            *nfds = max(*nfds, this_cmc_server->katcp_socket_fd);
            
            //now for the individual arrays.
            verbose_message(BORING, "Setting up fds on %ld arrays on %s:%hu.\n", this_cmc_server->no_of_arrays, this_cmc_server->address, this_cmc_server->katcp_port);
            size_t i;
            for (i = 0; i < this_cmc_server->no_of_arrays; i++)
            {
                array_set_fds(this_cmc_server->array_list[i], rd, wr, nfds);
            }
    }
}


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
                verbose_message(BORING, "Sending KATCP message \"%s\" to %s:%hu\n", composed_message, this_cmc_server->address, this_cmc_server->katcp_port);
                free(composed_message);
                composed_message = NULL;

                char *first_word = malloc(strlen(message_see_word(this_cmc_server->current_message, 0)) + 2);
                sprintf(first_word, "%c%s", message_get_type(this_cmc_server->current_message), message_see_word(this_cmc_server->current_message, 0));
                if (message_get_number_of_words(this_cmc_server->current_message) == 1)
                {
                    verbose_message(BORING, "It's a single-word message.\n");
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
                free(first_word);
                first_word = NULL;

                this_cmc_server->state = CMC_WAIT_RESPONSE;
            }
            else
            {
                verbose_message(WARNING, "A message on %s:%hu's queue had 0 words in it. Cannot send.\n", this_cmc_server->address, this_cmc_server->katcp_port);
                //TODO push through the queue if there's an error.
            }
        }
    }

    size_t i;
    for (i=0; i < this_cmc_server->no_of_arrays; i++)
    {
        array_setup_katcp_writes(this_cmc_server->array_list[i]);
    }
}


void cmc_server_socket_read_write(struct cmc_server *this_cmc_server, fd_set *rd, fd_set *wr)
{
    switch (this_cmc_server->state) {
        case CMC_WAIT_CONNECT:
            if (FD_ISSET(this_cmc_server->katcp_socket_fd, wr))
            {
                verbose_message(DEBUG, "%s:%hu file descriptor writeable.\n", this_cmc_server->address, this_cmc_server->katcp_port);
                int so_error;
                socklen_t socklen = sizeof(so_error);
                getsockopt(this_cmc_server->katcp_socket_fd, SOL_SOCKET, SO_ERROR, &so_error, &socklen);
                if (so_error == 0)
                {
                    //Connection is a success
                    verbose_message(DEBUG, "Connection successful.\n");
                    this_cmc_server->katcl_line = create_katcl(this_cmc_server->katcp_socket_fd);
                    this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                }
                else
                {
                    //Connection failed for whatever reason.
                    verbose_message(DEBUG, "Connection failed: %s\n", strerror(so_error));
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
                verbose_message(BORING, "Reading katcl_line from %s:%hu on fd %d.\n", this_cmc_server->address, this_cmc_server->katcp_port, this_cmc_server->katcp_socket_fd);
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
                verbose_message(BORING, "Writing katcl_line to %s:%hu on fd %d.\n", this_cmc_server->address, this_cmc_server->katcp_port, this_cmc_server->katcp_socket_fd);
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


static int cmc_server_add_array(struct cmc_server *this_cmc_server, char *array_name, uint16_t control_port, uint16_t monitor_port, size_t number_of_antennas)
{
    size_t i;
    for (i = 0; i < this_cmc_server->no_of_arrays; i++)
    {
        if (!strcmp(array_name, array_get_name(this_cmc_server->array_list[i])))
        {
            //Might want to think about comparing the other stuff as well, just in case for some reason the
            //original array got destroyed and another different one but called the same name snuck in.
            verbose_message(INFO, "Attempting to add array \"%s\" to %s:%hu while an array of this name already exists.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
            return (int) i;
        }
    }

    /* If not, allocate space for it on the end. */
    struct array **temp = realloc(this_cmc_server->array_list, sizeof(*(this_cmc_server->array_list))*(this_cmc_server->no_of_arrays + 1));
    if (temp == NULL)
    {
        verbose_message(ERROR, "Unable to realloc memory to add array \"%s\" to %s:%hu.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
        return -1;
    }
    this_cmc_server->array_list = temp;
    this_cmc_server->array_list[this_cmc_server->no_of_arrays] = array_create(array_name, this_cmc_server->address, control_port, monitor_port, number_of_antennas);
    if (this_cmc_server->array_list[this_cmc_server->no_of_arrays] == NULL)
    {
        verbose_message(ERROR, "Unable to create array \"%s\" on %s:%hu.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
        return -1;
    }
    verbose_message(INFO, "Added array \"%s\" to %s:%hu.\n", array_name, this_cmc_server->address, this_cmc_server->katcp_port);
    this_cmc_server->no_of_arrays++;
    return 0;
}


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
        verbose_message(BORING, "From %s:%hu: %s %s %s %s %s\n", this_cmc_server->address, this_cmc_server->katcp_port, \
                arg_string_katcl(this_cmc_server->katcl_line, 0), \
                arg_string_katcl(this_cmc_server->katcl_line, 1), \
                arg_string_katcl(this_cmc_server->katcl_line, 2), \
                arg_string_katcl(this_cmc_server->katcl_line, 3), \
                arg_string_katcl(this_cmc_server->katcl_line, 4)); 
        char received_message_type = arg_string_katcl(this_cmc_server->katcl_line, 0)[0];
        switch (received_message_type) {
            case '!': // it's a katcp response
                if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 0) + 1, message_see_word(this_cmc_server->current_message, 0)))
                {
                    if (!strcmp(arg_string_katcl(this_cmc_server->katcl_line, 1), "ok"))
                    {
                        verbose_message(DEBUG, "%s:%hu received %s ok!\n",\
                                this_cmc_server->address, this_cmc_server->katcp_port, message_see_word(this_cmc_server->current_message, 0));
                        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
                        verbose_message(BORING, "%s:%hu still has %u message(s) in its queue...\n",\
                                this_cmc_server->address, this_cmc_server->katcp_port, queue_sizeof(this_cmc_server->outgoing_msg_queue));
                        if (queue_sizeof(this_cmc_server->outgoing_msg_queue))
                        {
                            verbose_message(BORING, "%s:%hu  popping queue...\n", this_cmc_server->address, this_cmc_server->katcp_port);
                            cmc_server_queue_pop(this_cmc_server);
                        }
                        else
                        {
                            verbose_message(INFO, "%s:%hu going into monitoring state.\n", this_cmc_server->address, this_cmc_server->katcp_port);
                            message_destroy(this_cmc_server->current_message);
                            this_cmc_server->current_message = NULL; //doesn't do this in the above function. C problem.
                            this_cmc_server->state = CMC_MONITOR;
                        }
                    }
                    else 
                    {
                        verbose_message(WARNING, "Received %s %s. Retrying the request...",\
                                message_see_word(this_cmc_server->current_message, 0), arg_string_katcl(this_cmc_server->katcl_line, 1));
                        this_cmc_server->state = CMC_SEND_FRONT_OF_QUEUE;
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
                }
                break;
            default:
                verbose_message(WARNING, "Unexpected KATCP message received, starting with %c\n", received_message_type);
        }
    }
}


/* CMC server will be represented as an H1 with a table of the arrays beneath it.
 * for the time being, just its name.
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


int cmc_server_check_for_array(struct cmc_server *this_cmc_server, char *array_name)
{
    size_t i;
    for (i = 0; i < this_cmc_server->no_of_arrays; i++)
    {
        if (!strcmp(array_name, array_get_name(this_cmc_server->array_list[i])))
        {
            return (int) i;
        }
    }
    return -1;
}


struct array *cmc_server_get_array(struct cmc_server *this_cmc_server, size_t array_number)
{
    if (array_number < this_cmc_server->no_of_arrays)
        return this_cmc_server->array_list[array_number];
    else
        return NULL;
}
