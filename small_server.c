/*
 * small_server.c
 * Author: James Smith
 * Date: 15 March 2019
 *
 * This is a simple program to listen on a given port, read whatever comes from that port and print to stdout.
 *
 * The purpose was to let me figure out both how to use sockets in c, and how to use select(). Turns out,
 * barring a few things to be careful for, it's a pretty straightforward process.
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
#include "html_handling.h"

/* This is handy for keeping track of the number of file descriptors. */
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#define BUF_SIZE 1024
#define NAME_LENGTH 1024


enum program_state {
    STARTUP_SEND_CLIENT_CONFIG,
    STARTUP_WAIT_CLIENT_CONFIG_OKAY,
    STARTUP_REQUEST_ARRAY_LIST,
    STARTUP_RECV_ARRAY_LIST,
    MONITOR,
    ADD_TO_LIST_REQUEST_MONITOR_PORT,
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
    int file_descriptors[FD_SETSIZE]; /* We can handle this many connections at once. */
    int file_descriptors_want_data[FD_SETSIZE]; /* Keep track of the fds that actually want something. */
    FILE *template_file;
    int i; /* for use as a loop index */
    struct katcl_line *l;
    int r; /* dump variable for results of functions */

    enum program_state state = STARTUP_SEND_CLIENT_CONFIG;

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
        l = create_katcl(katcp_socket_fd);
   
    /* Clear out the array of file descriptors. */
    for (i = 0; i < FD_SETSIZE; i++)
    {
        file_descriptors[i] = -1; /* -1 indicates that it should be ignored. */
        file_descriptors_want_data[i] = 0;
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

        /* we only need to write to it in one of a certain number of states */
        switch (state)
        {
            case STARTUP_SEND_CLIENT_CONFIG:
            case STARTUP_REQUEST_ARRAY_LIST:
            case ADD_TO_LIST_REQUEST_MONITOR_PORT:
                FD_SET(katcp_socket_fd, &wr);
                /* nfds already updated to include this fd, so no need to update it again here. */
                break;
           default:
                ;
        }
            
        /* This sets up which file descriptors we'd like to read from on this round. */
        for (i = 0; i < FD_SETSIZE; i++)
        {
            if (file_descriptors[i] > 0)
            {
                FD_SET(file_descriptors[i], &rd);
                FD_SET(file_descriptors[i], &er); /* For completeness... */
                if (file_descriptors_want_data[i])
                    FD_SET(file_descriptors[i], &wr);
                nfds = max(nfds, file_descriptors[i]);
            }
        }

        for (i = 0; i < array_list_size; i++)
        {
            switch (array_list[i]->state)
            {
                case REQUEST_SENSOR_LISTS:
                    for (i = 0; i < array_list_size; i++)
                    {
                        FD_SET(array_list[i]->monitor_socket_fd, &wr);
                        nfds = max(nfds, array_list[i]->monitor_socket_fd);
                    }
                    break;
                case RECEIVE_SENSOR_LISTS:
                    for (i = 0; i < array_list_size; i++)
                    {
                        FD_SET(array_list[i]->monitor_socket_fd, &rd);
                        nfds = max(nfds, array_list[i]->monitor_socket_fd);
                    }
                    break;
                default:
                    ;
            }
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
                for (i = 0; i < FD_SETSIZE; i++)
                {
                    if (file_descriptors[i] < 0)
                    {
                        file_descriptors[i] = r;
                        break;
                    }
                } 
                if (i == FD_SETSIZE)
                    perror("Unable to open additional connections."); /* I have a suspicion that we wont' actually get here because accept() will likely return an error if there are too many file descriptors already open? */
                else
                    printf("Connection from %s\n", inet_ntoa(client_address.sin_addr));
            }
        }

        if (FD_ISSET(katcp_socket_fd, &wr)) /* Tell the cmc that we'd like to know something about the arrays that are here. */
        {
            int send = 0; /* flag to skip over sending if there's nothing to send. */
            switch (state)
            {
                case STARTUP_SEND_CLIENT_CONFIG:
                    append_string_katcl(l, KATCP_FLAG_FIRST, "?client-config");
                    append_string_katcl(l, KATCP_FLAG_LAST, "info-all");
                    send = 1;
                    state = STARTUP_WAIT_CLIENT_CONFIG_OKAY;
                    break;
                case STARTUP_REQUEST_ARRAY_LIST:
                    append_string_katcl(l, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, "?array-list");
                    send = 1;
                    state = STARTUP_RECV_ARRAY_LIST;
                    break;
                case ADD_TO_LIST_REQUEST_MONITOR_PORT:
                    append_string_katcl(l, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, "?array-list");
                    send = 1;
                    state = ADD_TO_LIST_RECEIVE_MONITOR_PORT;
                    break;

                default: 
                    send = 0; /* just in case */
            }
            if (send) 
            {
                r = write_katcl(l);
                if (r < 0)
                {
                    fprintf(stderr, "failed to send katcp message\n");
                    perror("write_katcl");
                    destroy_katcl(l, 1);
                }
            }
        }

        if (FD_ISSET(katcp_socket_fd, &rd))
        {
           r = read_katcl(l);
           if (r)
           {
               fprintf(stderr, "read failed: %s\n", (r < 0) ? strerror(error_katcl(l)) : "connection terminated");
               perror("read_katcl");
           }

           while (have_katcl(l) > 0)
           {
                switch (state)
                {
                    case STARTUP_WAIT_CLIENT_CONFIG_OKAY:
                        if (!strcmp(arg_string_katcl(l, 0), "!client-config"))
                        {
                            if (!strcmp(arg_string_katcl(l, 1), "ok"))
                                state = STARTUP_REQUEST_ARRAY_LIST;
                            else
                            {
                                fprintf(stderr, "!client-config received %s, expected ok\nretrying...\n", arg_string_katcl(l, 1));
                                state = STARTUP_SEND_CLIENT_CONFIG;
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
                            printf("Finished getting initial array list. Getting sensor lists...\n");
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
                            state = ADD_TO_LIST_REQUEST_MONITOR_PORT;
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
        }

        /* Handle the OOB stuff first. For completeness' sake. */
        for (i = 0; i < FD_SETSIZE; i++)
        {
            if (file_descriptors[i] > 0)
            {
                if (FD_ISSET(file_descriptors[i], &er))
                {
                    char c;
                    r = recv(file_descriptors[i], &c, 1, MSG_OOB);
                    if (r < 1)
                    {
                        shutdown(file_descriptors[i], SHUT_RDWR);
                        close(file_descriptors[i]);
                        file_descriptors[i] = -1;
                    }
                    else
                        printf("OOB: %c\n", c);
                }
            }
        }

        /* Now handle the actual reading from the socket. */
        for (i = 0; i < FD_SETSIZE; i++)
        {
            if (file_descriptors[i] > 0)
            {
                if (FD_ISSET(file_descriptors[i], &rd))
                {
                    r = read(file_descriptors[i], buffer, BUF_SIZE - 1); /* -1 to prevent overrunning the buffer. */
                    if (r < 1)
                    {
                        shutdown(file_descriptors[i], SHUT_RDWR);
                        close(file_descriptors[i]);
                        file_descriptors[i] = -1;
                    }
                    else
                    {
                        buffer[r] = '\0'; /* To make it a well-formed string. */
                        if (strncmp(buffer, "GET", 3) == 0)
                        {
                            file_descriptors_want_data[i] = 1;
                            //strtok(buffer, " ");
                            printf("buffer: %s\n", buffer);
                            /*TODO
                             * check through the monitor ports of the arrays currenly being watched, or the array_names perhaps?
                             * then give the want_data the correct identifier - this will probably be the monitor port because it's also an int.
                             * shunt the stuff down. */
                        }
                        else
                            printf("Got a message not starting with GET: %s\n", buffer);
                    }
                }
            }
        }

        /* Handle the sockets that want to be written to. */
        for (i = 0; i < FD_SETSIZE; i++)
        {
            if (file_descriptors[i] > 0)
            {
                if (FD_ISSET(file_descriptors[i], &wr))
                {  
                    r = send_http_ok(file_descriptors[i]);
                    r = send_html_header(file_descriptors[i]);
                    r = send_html_body_open(file_descriptors[i]);

                    if (array_list)
                    {
                        send_html_table_start(file_descriptors[i]);
                        send_html_table_arraylist_header(file_descriptors[i]);
                        int j;
                        for (j = 0; j < array_list_size; j++)
                        {
                            send_html_table_arraylist_row(file_descriptors[i], array_list[j]);
                        }
                        send_html_table_end(file_descriptors[i]);
                    }
                    else
                    {
                        char message[] = "No arrays currently running, or cmc not yet polled. Please try again later...\n";
                        /* TODO make this auto-refreshing as well.*/
                        r = write(file_descriptors[i], message, sizeof(message));
                    }

                    r = send_html_body_close(file_descriptors[i]);

                    shutdown(file_descriptors[i], SHUT_RDWR);
                    close(file_descriptors[i]);
                    file_descriptors[i] = -1;
                    file_descriptors_want_data[i] = 0;
                }
            }
        }

        for (i = 0; i < array_list_size; i++)
        {
            if (FD_ISSET(array_list[i]->monitor_socket_fd, &wr))
            {
                switch (array_list[i]->state)
                {
                    case REQUEST_SENSOR_LISTS:
                        request_sensor_list(array_list[i]);
                        array_list[i]->state = RECEIVE_SENSOR_LISTS;
                        break;
                    default:
                        ;
                }
            }

            if (FD_ISSET(array_list[i]->monitor_socket_fd, &rd))
            {
                switch (array_list[i]->state)
                {
                    case RECEIVE_SENSOR_LISTS:
                        r = accept_sensor_list(array_list[i]);
                        if (!r) /* i.e. if r was zero */
                            array_list[i]->state = MONITOR_SENSORS;
                        break;
                    default:
                        ;
                }
            }
        }
    }

    /* cleanup */
    destroy_katcl(l, 1);
    for (i = 0; i < array_list_size; i++)
        destroy_array(array_list[i]);
    free(array_list);

    return 0;
}

