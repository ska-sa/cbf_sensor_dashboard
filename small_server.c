/*
 * small_server.c
 * Author: James Smith
 *
 * Simple program to display an html-dashboard monitoring the state of the sensors in the correlator.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h> /* Needed for read() and write() */
#include <signal.h>
#include <katcp.h>
#include <katcl.h>
#include <netc.h>

#include "array_handling.h"
#include "http_handling.h"
#include "html_handling.h"

/* This is handy for keeping track of the number of file descriptors. */
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#define BUF_SIZE 1024
#define NAME_LENGTH 1024


enum program_state {
    STARTUP_WAIT_CLIENT_CONFIG_OKAY,
    STARTUP_RECV_ARRAY_LIST,
    MONITOR,
    ADD_TO_LIST_RECEIVE_MONITOR_PORT
};

volatile sig_atomic_t stop = 0;
void sigint(int signo)
{
    if (signo == SIGINT)
    {
        printf("Caught Ctrl+C, exiting...\n");
        stop = 1;
    }
}

int main(int argc, char *argv[])
{

    signal(SIGINT, &sigint);

    int listening_port, katcp_port;
    char *cmc_address;
    int server_socket_fd, katcp_socket_fd;
    char buffer[BUF_SIZE];

    struct webpage_client **client_list = NULL;
    int client_list_size = 0;

    FILE *template_file;
    int i; /* for use as a loop index */
    struct katcl_line *l;
    int r; /* dump variable for results of functions */

    enum program_state state = STARTUP_WAIT_CLIENT_CONFIG_OKAY;

    struct cmc_array **array_list = NULL;
    int array_list_size = 0;
    char* new_array_name = NULL;
//    int make_new_array_list = 0;

    /* argc is always one more than the number of arguments passed, because the first one
     * is the name of the executable. */
    if (argc != 5)
    {
        fprintf(stderr, "%s\n", "Usage:\n\tserver <listen-port> <html-template-file> <katcp-server> <katcp-port>");
        exit(EXIT_FAILURE);
    }

    /* open a socket on the port specified */
    listening_port = atoi(argv[1]);
    server_socket_fd = listen_on_socket(listening_port);
    if (server_socket_fd == -1)
    {
        fprintf(stderr, "Unable to create socket on port %d\n", listening_port);
        exit(EXIT_FAILURE);
    }

    template_file = fopen(argv[2], "r");
    if (template_file == NULL)
    {
        fprintf(stderr, "Unable to open HTML template file %s\n", argv[2]);
        exit(EXIT_FAILURE);
    }
    fclose(template_file); /* Will open it again later. */

    katcp_port = atoi(argv[4]);
    cmc_address = malloc(strlen(argv[3]) + 1);
    sprintf(cmc_address, "%s", argv[3]);
    katcp_socket_fd = net_connect(cmc_address, katcp_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
    if (katcp_socket_fd == -1)
    {
        fprintf(stderr, "Unable to connect to katcp server %s:%d\n", argv[3], katcp_port);
        exit(EXIT_FAILURE);
    }
    else
    {
        l = create_katcl(katcp_socket_fd);
        append_string_katcl(l, KATCP_FLAG_FIRST, "?client-config");
        append_string_katcl(l, KATCP_FLAG_LAST, "info-all");
    }
   
    while(!stop) /* to allow for graceful exiting, otherwise we get potential leaks. */
    {
        int nfds = 0; /* number of file descriptors. */
        fd_set rd, wr, er; /* sets of file descriptors to monitor things. er will likely not be used but
                              marc says it's good practice to keep it around. */

        /* clear all the fd_sets */
        FD_ZERO(&rd);
        FD_ZERO(&wr);
        FD_ZERO(&er);

        /* Indicate that we're happy to read from the socket that we've just opened. */
        FD_SET(server_socket_fd, &rd);
        nfds = max(nfds, server_socket_fd);
        
        /* we're also happy to read from the katcp socket. */
        FD_SET(katcp_socket_fd, &rd);
        nfds = max(nfds, katcp_socket_fd);

        /* we only need to write to the socket if there's something queued */
        if (flushing_katcl(l))
            FD_SET(katcp_socket_fd, &wr);
            
        /* we're interested in reading from any connected client, and we might have something to write to them */
        for (i = 0; i < client_list_size; i++)
        {
            FD_SET(client_list[i]->fd, &rd);
            if (have_buffer_to_write(client_list[i]->buffer))
                FD_SET(client_list[i]->fd, &wr);
            nfds = max(nfds, client_list[i]->fd);
        }

        /* katcp sockets of individual subarrays */
        for (i = 0; i < array_list_size; i++)
        {
            if (flushing_katcl(array_list[i]->l)) /* only write if there's something queued. */
            {
                FD_SET(array_list[i]->monitor_socket_fd, &wr);
            }
            FD_SET(array_list[i]->monitor_socket_fd, &rd); /* but listen for reading on all */
            nfds = max(nfds, array_list[i]->monitor_socket_fd);
        }

        /* Passing select() a NULL as the last parameter blocks here, and can even be swapped,
         * until such time as there's something to be done on one of the file-descriptors*/
        //printf("Heading into select with state  %d\n", state);
        r = select(nfds + 1, &rd, &wr, &er, NULL);
        //printf("Selected %d, currently in state %d\n", r, state);


        /* EINTR just means it was interrupted by a signal or something.
         * Ignore and try again on the next round. */
        if (r == -1 && errno == EINTR)
            continue;
        
        /* This is an actual problem. */
        if (r == -1)
        {
            perror("select()");
            exit(EXIT_FAILURE);
        }

        /* If we've decided in the above section to read from the socket... */
        if (FD_ISSET(server_socket_fd, &rd))
        {
            unsigned int len;
            struct sockaddr_in client_address;
    
            /* accept (i.e. open) the connection, get a new file descriptor for the open connection. */
            memset(&client_address, 0, len = sizeof(client_address));
            r = accept(server_socket_fd, (struct sockaddr *) &client_address, &len);
            if (r == -1)
            {
                perror("accept()");
            }
            else
            {
                struct webpage_client **temp = realloc(client_list, sizeof(*client_list)*(++client_list_size));
                if (temp)
                {
                    client_list = temp;
                    client_list[client_list_size-1] = create_webpage_client(r);
                    printf("webpage client created, list size is now %d\n", client_list_size);
                }
                else
                {
                    shutdown(r, SHUT_RDWR);
                    close(r);
                    fprintf(stderr, "Unable to allocate memory for another web client\n");
                    perror("realloc");
                }
            }
        }


        /* handle reading and writing of the katcp sockets */
        if (FD_ISSET(katcp_socket_fd, &rd))
        {
           r = read_katcl(l);
           if (r)
           {
               fprintf(stderr, "read failed: %s\n", (r < 0) ? strerror(error_katcl(l)) : "connection terminated");
               perror("read_katcl");
           }
        }
        if (FD_ISSET(katcp_socket_fd, &wr)) /* Tell the cmc that we'd like to know something about the arrays that are here. */
        {
            r = write_katcl(l);
            if (r < 0)
            {
                fprintf(stderr, "failed to send katcp message\n");
                perror("write_katcl");
                destroy_katcl(l, 1);
            }
        }

            
       /* first if we have some katcl to read */
       if (have_katcl(l) > 0)
       {
            switch (state)
            {
                case STARTUP_WAIT_CLIENT_CONFIG_OKAY:
                    if (!strcmp(arg_string_katcl(l, 0), "!client-config"))
                    {
                        if (!strcmp(arg_string_katcl(l, 1), "ok"))
                        {
                            append_string_katcl(l, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, "?array-list");
                            state = STARTUP_RECV_ARRAY_LIST;
                        }
                        else
                        {
                            fprintf(stderr, "!client-config received %s, expected ok\nretrying...\n", arg_string_katcl(l, 1));
                            append_string_katcl(l, KATCP_FLAG_FIRST, "?client-config");
                            append_string_katcl(l, KATCP_FLAG_LAST, "info-all");
                        }
                    }
                    break;
                case STARTUP_RECV_ARRAY_LIST:
                    if (!strcmp(arg_string_katcl(l, 0), "#array-list"))
                    {
                        struct cmc_array **temp = realloc(array_list, sizeof(*array_list)*(++array_list_size));
                        if (temp)
                            array_list = temp;
                        char* array_name = arg_string_katcl(l, 1);
                        strtok(arg_string_katcl(l, 2), ","); /* don't need the first bit, that's the control port */
                        int monitor_port = atoi(strtok(NULL, ","));
                        printf("Monitoring array \"%s\" on port %d of the CMC...\n", array_name, monitor_port);
                        int j = 3;
                        char *multicast_groups = malloc(1);
                        multicast_groups[0] = '\0';
                        char *buffer;
                        do {
                            buffer = arg_string_katcl(l, j);
                            if (buffer)
                            {
                                multicast_groups = realloc(multicast_groups, strlen(multicast_groups) + strlen(buffer) + 2);
                                strcat(multicast_groups, " ");
                                strcat(multicast_groups, buffer);
                            }
                            j++;
                        } while (buffer);
                        array_list[array_list_size-1] = create_array(array_name, monitor_port, multicast_groups, cmc_address);
                        free(multicast_groups);
                    }
                    else if (!strcmp(arg_string_katcl(l, 0), "!array-list"))
                    {
                        printf("Finished getting initial array list. Watching for any new ones...\n");
                        state = MONITOR;
                    }
                    break;
                case MONITOR:
                    if (!strcmp(arg_string_katcl(l, 0), "#group-created"))
                    {
                        char *temp = arg_string_katcl(l, 1);
                        new_array_name = malloc(strlen(temp));
                        sprintf(new_array_name, "%s",  strtok(temp, ".")); /* array name will be format steven.control or steven.monitor, so we only need the steven part */
                        /* this needs to be sprintf to ensure that the name persists once this katcl line is moved on to the next thing. */
                        printf("Noticed a new array, %s. Requesting monitor port to connect to...\n", new_array_name);
                        append_string_katcl(l, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, "?array-list");
                        state = ADD_TO_LIST_RECEIVE_MONITOR_PORT;
                        break;
                    }
                    else if (!strcmp(arg_string_katcl(l, 0), "#group-destroyed"))
                    {
                        /* remove the array from the list. */
                        char *name_of_removed_array = strtok(arg_string_katcl(l, 1), ".");
                        char *temp = strtok(NULL, ".");
                        if (!strcmp(temp, "monitor")) /* ignore the "control" one, otherwise we remove it first time round and get an unexpected message when the other group is destroyed. */
                        { 
                            int j;
                            for (j = 0; j < array_list_size; j++)
                            {
                                if (!strcmp(name_of_removed_array, array_list[j]->name))
                                    break;
                            }
                            if (j == array_list_size)
                                fprintf(stderr, "Weirdly, the CMC said that %s is being destroyed, but we didn't have it on the list in the first place...", name_of_removed_array);
                            else
                            {
                                destroy_array(array_list[j]);
                                memmove(&array_list[j], &array_list[j+1], (array_list_size - j - 1)*sizeof(*array_list));
                                struct cmc_array **temp = realloc(array_list, sizeof(*array_list)*(--array_list_size));
                                if (temp)
                                    array_list = temp;
                                printf("No longer monitoring %s - destroyed.\n", name_of_removed_array);
                                //free(name_of_removed_array);
                            }
                        }
                    }
                    break;
                case ADD_TO_LIST_RECEIVE_MONITOR_PORT:
                    if (!strcmp(arg_string_katcl(l, 0), "#array-list") && !(strcmp(arg_string_katcl(l, 1), new_array_name)))
                    {
                        printf("Caught the list entry with our new array, %s\n", new_array_name);
                        struct cmc_array **temp = realloc(array_list, sizeof(*array_list)*(++array_list_size));
                        if (temp)
                            array_list = temp;
                        //char* temp_array_name = arg_string_katcl(l, 1);
                        strtok(arg_string_katcl(l, 2), ",");
                        int monitor_port = atoi(strtok(NULL, ","));
                        printf("Monitoring array \"%s\" on port %d of the CMC...\n", new_array_name, monitor_port);

                        int j = 3;
                        char *multicast_groups = malloc(1);
                        multicast_groups[0] = '\0';
                        char *buffer;
                        do {
                            buffer = arg_string_katcl(l, j);
                            if (buffer)
                            {
                                multicast_groups = realloc(multicast_groups, strlen(multicast_groups) + strlen(buffer) + 2);
                                strcat(multicast_groups, " ");
                                strcat(multicast_groups, buffer);
                            }
                            j++;
                        } while (buffer);
                        array_list[array_list_size-1] = create_array(new_array_name, monitor_port, multicast_groups, cmc_address);

                        free(new_array_name);
                        free(multicast_groups);
                        state = MONITOR;
                        break;
                    }
               default:
                    ;
            }
        }
       
        /* Now handle the actual reading from the socket. */
        for (i = 0; i < client_list_size; i++)
        {
            if (FD_ISSET(client_list[i]->fd, &rd))
            {
                r = read(client_list[i]->fd, buffer, BUF_SIZE - 1); /* -1 to prevent overrunning the buffer. */
                if (r < 0)
                {
                    /*TODO add the erro things. */
                    destroy_webpage_client(client_list[i]);
                    memmove(&client_list[i], &client_list[i+1], (client_list_size - i - 1)*sizeof(*client_list));
                    struct webpage_client **temp = realloc(client_list, sizeof(*client_list)*(--client_list_size));
                    if (temp)
                        client_list = temp;
                    printf("client disconnected, list size is now %d\n", client_list_size);
                    i--;
                }
                else if (r > 0)
                {
                    buffer[r] = '\0'; /* To make it a well-formed string. */
                    char first_word[BUF_SIZE];
                    sprintf(first_word, "%s", strtok(buffer, " "));
                    if (strcmp(first_word, "GET") == 0)
                    {
                        r = send_html_header(client_list[i]->buffer);
                        r = send_html_body_open(client_list[i]->buffer);

                        char *requested_resource = strtok(NULL, " ");
                        if (strcmp(requested_resource, "/") == 0)
                        {
                            if (array_list)
                            {
                                send_html_table_start(client_list[i]->buffer);
                                send_html_table_arraylist_header(client_list[i]->buffer);
                                int j;
                                for (j = 0; j < array_list_size; j++)
                                {
                                    send_html_table_arraylist_row(client_list[i]->buffer, array_list[j]);
                                }
                                send_html_table_end(client_list[i]->buffer);
                            }
                            else
                            {
                                char message[] = "No arrays currently running, or cmc not yet polled. Please try again later...\n";
                                r = send_html_paragraph(client_list[i]->buffer, message);
                            }
                        }
                        else
                        {
                            int j;
                            for (j = 0; j < array_list_size; j++)
                            {
                                if (strcmp(requested_resource + 1, array_list[j]->name) == 0)
                                {
                                    send_html_table_start(client_list[i]->buffer);
                                    int k;
                                    for (k = 0; k < array_list[j]->number_of_antennas; k++)
                                    {
                                        send_html_table_sensor_row(client_list[i]->buffer, array_list[j]->fhosts[k], array_list[j]->xhosts[k]);
                                    }
                                    send_html_table_end(client_list[i]->buffer);
                                    break; /* found the array, no need to continue further */
                                }
                            }
                            if (j == array_list_size)
                            {
                                size_t needed = snprintf(NULL, 0, "Array %s not found running at the moment.", requested_resource + 1) + 1;
                                char *message = malloc(needed);
                                sprintf(message, "Array %s not found running at the moment.", requested_resource + 1);
                                r = send_html_paragraph(client_list[i]->buffer, message);
                                free(message);
                            }

                        }

                        r = send_html_body_close(client_list[i]->buffer);
                    }
                }
                else
                    ;
            }
        }

        /* Handle the sockets that want to be written to. */
        for (i = 0; i < client_list_size; i++)
        {
            if (FD_ISSET(client_list[i]->fd, &wr))
            {  
                r = write_buffer_to_fd(client_list[i]->fd, client_list[i]->buffer, BUF_SIZE);
                if (r < 0)
                {
                    /*TODO this is error handling I think, -1 is an error, 0 is finished. */
                    shutdown(client_list[i]->fd, SHUT_RDWR);
                    destroy_webpage_client(client_list[i]);
                    memmove(&client_list[i], &client_list[i+1], (client_list_size - i - 1)*sizeof(*client_list));
                    struct webpage_client **temp = realloc(client_list, sizeof(*client_list)*(--client_list_size));
                    if (temp)
                        client_list = temp;
                    printf("client disconnected, list size is now %d\n", client_list_size);
                }
            }
        }

        /* Individual array sensor servlet monitoring */
        for (i = 0; i < array_list_size; i++)
        {
            if (FD_ISSET(array_list[i]->monitor_socket_fd, &wr))
            {
                r = write_katcl(array_list[i]->l);
                if (r < 0)
                {
                    fprintf(stderr, "failed to send katcp message\n");
                    perror("write_katcl");
                    destroy_katcl(l, 1);
                }
                
            }

            if (FD_ISSET(array_list[i]->monitor_socket_fd, &rd))
            {
               r = read_katcl(array_list[i]->l);
               if (r)
               {
                   fprintf(stderr, "read failed: %s\n", (r < 0) ? strerror(error_katcl(array_list[i]->l)) : "connection terminated");
                   perror("read_katcl");
               }
            }
            
            if (have_katcl(array_list[i]->l) > 0)
            {
                if (!strcmp(arg_string_katcl(array_list[i]->l, 0), "#log"))
                {
                    ; /*Just explicitly ignore the log, so we don't have to waste cycles comparing. */
                }
                else if (!strcmp(arg_string_katcl(array_list[i]->l, 0), "#sensor-status"))
                {
                    /*TODO: this is where the sensor value would need to be updated.*/
                    process_sensor_status(array_list[i]);
                }
                else
                {
                    switch (array_list[i]->state)
                    {
                        case REQUEST_FUNCTIONAL_MAPPING: /*Ask for the hostname-functional-mapping sensor */
                            r = request_functional_mapping(array_list[i]);
                            if (r >= 0)
                                array_list[i]->state = RECEIVE_FUNCTIONAL_MAPPING;
                            break;
                        case RECEIVE_FUNCTIONAL_MAPPING:
                            r = accept_functional_mapping(array_list[i]);
                            if (r == 0) 
                            {
                                r = request_sensor_fhost_device_status(array_list[i]);
                                array_list[i]->state = RECEIVE_SENSOR_FHOST_DEVICE_STATUS_RESPONSE;
                            }
                            break;
                        case RECEIVE_SENSOR_FHOST_DEVICE_STATUS_RESPONSE:
                            r = receive_sensor_fhost_device_status_response(array_list[i]);
                            if (r == 0)
                            {
                                if (array_list[i]->host_counter == array_list[i]->number_of_antennas)
                                {
                                    array_list[i]->state = MONITOR_SENSORS;
                                    array_list[i]->host_counter = 0;
                                }
                                else
                                {
                                    ; /* not sure what to do if this condition is reached. */
                                }
                            }
                            break;
                        default:
                            ;
                    }
                }
            }
        }
    }

    /* cleanup */
    destroy_katcl(l, 1);
    for (i = 0; i < array_list_size; i++)
        destroy_array(array_list[i]);
    if (array_list_size)
        free(array_list);

    for (i = 0; i < client_list_size; i++)
        destroy_webpage_client(client_list[i]);
    if (client_list_size)
        free(client_list);

    free(cmc_address);

    return 0;
}

