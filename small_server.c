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

/* This is handy for keeping track of the number of file descriptors. */
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

#define BUF_SIZE 1024
#define NAME_LENGTH 1024

volatile sig_atomic_t stop = 0;
void sigint(int signo)
{
    if (signo == SIGINT)
    {
        printf("Caught Ctrl+C, exiting...\n");
        stop = 1;
    }
}

/* Function takes a port number as an argument and returns a file descriptor
 * to the resulting socket. Opens socket on 0.0.0.0. */
static int listen_on_socket(int listening_port)
{
    struct sockaddr_in a;
    int s;
    int yes;

    if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("socket");
        return -1;
    }

    /* This is an eccentricity of setsockopt, it needs an address and not just a value for the "1",
     * so you give it this "yes" variable.*/
    yes = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1)
    {
        perror("setsockopt");
        close(s);
        return -1;
    }
    /* clear out the address struct from whatever garbage was in it. */
    memset(&a, 0, sizeof(a));
    a.sin_port = htons(listening_port);
    a.sin_family = AF_INET;
    /* TODO - explicitly make the addr point to 0.0.0.0? I guess it's not really needed. */
    if (bind(s, (struct sockaddr *) &a, sizeof(a)) == -1)
    {
        perror("bind");
        close(s);
        return -1;
    }
    printf("Accepting connections on port %d\n", listening_port);
    listen(s, 10); /* turns out 10 is a thumb-suck value but it's pretty sane. A legacy of olden times... */
    return s;
}

int main(int argc, char *argv[])
{

    signal(SIGINT, &sigint);

    int listening_port, katcp_port;
    int server_socket_fd, katcp_socket_fd;
    char buffer[BUF_SIZE];
    int file_descriptors[FD_SETSIZE]; /* We can handle this many connections at once. */
    char file_descriptors_want_data[FD_SETSIZE]; /* Keep track of the fds that actually want something. */
    FILE *template_file;
    int i; /* for use as a loop index */
    struct katcl_line *l;
    int r; /* dump variable for results of functions */

    struct cmc_array **array_list = NULL;
    int array_list_size = 0;
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
    katcp_socket_fd = net_connect(argv[3], katcp_port, NETC_VERBOSE_ERRORS | NETC_VERBOSE_STATS);
    if (katcp_socket_fd == -1)
    {
        fprintf(stderr, "Unable to connect to katcp server %s:%d\n", argv[3], katcp_port);
        exit(EXIT_FAILURE);
    }
    else
    /* Tell the cmc that we'd like to know something about the arrays that are here. */
    {
        l = create_katcl(katcp_socket_fd);
        if (append_string_katcl(l, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, "?array-list") < 0) return -1;
        r = write_katcl(l);
    }
    
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

        /* Passing select() a NULL as the last parameter blocks here, and can even be swapped,
         * until such time as there's something to be done on one of the file-descriptors*/
        r = select(nfds + 1, &rd, &wr, &er, NULL);

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
            unsigned int l;
            struct sockaddr_in client_address;
    
            /* accept (i.e. open) the connection, get a new file descriptor for the open connection. */
            memset(&client_address, 0, l = sizeof(client_address));
            r = accept(server_socket_fd, (struct sockaddr *) &client_address, &l);
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

        if (FD_ISSET(katcp_socket_fd, &wr))
        {
            if (l == NULL)
                l = create_katcl(katcp_socket_fd);
            /* Need to determine which message to send. It'll be an array-list first, sensor-list and sensor-sampling later. */
            if (append_string_katcl(l, KATCP_FLAG_FIRST | KATCP_FLAG_LAST, "?array-list") < 0) perror("append_string_katcl");
            r = write_katcl(l);
            if (r < 0)
            {
                fprintf(stderr, "failed to send katcp message\n");
                perror("write_katcl");
                destroy_katcl(l, 1);
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
               if (strncmp(arg_string_katcl(l, 0), "#log", 4) != 0) /* i.e. if it's not a log */
               {
                   /* This is where the tricky bit comes in. */
                   //printf("Received something that's not a log.\n");
                   char *buffer = read_full_katcp_line(l);
                   printf("%s\n", buffer);
                   free(buffer);
               }
           }
           //printf("Current array_list_size: %d\n", array_list_size);
           for (i = 0; i < array_list_size; i++)
           {
               char* buffer = get_array_name(array_list[i]);
               printf("line %d: %s\n", i, buffer);
               free(buffer);
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
                            file_descriptors_want_data[i] = 1;
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
                    /* commenting this out for the time being.
                    template_file = fopen(argv[2], "r");
                    if (template_file == NULL)
                    {
                        char error_message[] = "HTTP/1.1 404 Not Found";
                        r = write(file_descriptors[i], error_message, sizeof(error_message));
                        shutdown(file_descriptors[i], SHUT_RDWR);
                        close(file_descriptors[i]);
                        file_descriptors[i] = -1;
                        file_descriptors_want_data[i] = 0;
                        fprintf(stderr, "Unable to open HTML template file %s\n", argv[2]);
                    }

                    char buffer[BUF_SIZE];
                    char *res;
                    do {
                        res = fgets(buffer, BUF_SIZE, template_file);
                        if (res != NULL)
                        {
                            r = write(file_descriptors[i], buffer, strlen(buffer));
                            if (r < 1)
                            {
                                shutdown(file_descriptors[i], SHUT_RDWR);
                                close(file_descriptors[i]);
                                file_descriptors[i] = -1;
                                file_descriptors_want_data[i] = 0;
                            }
                        }
                    } while (res != NULL);

                    fclose(template_file);
                    */
                    
                    if (array_list)
                    {
                        /*int j;
                        for (j = 0; j < array_list_size; j++)
                        {
                           char *line_to_write;
                            size_t needed = snprintf(NULL, 0, "%s %d\n", array_list[j]->name, array_list[j]->monitor_port) + 1;
                            line_to_write = malloc(needed);
                            sprintf(line_to_write, "%s %d\n", array_list[j]->name, array_list[j]->monitor_port);
                            r = write(file_descriptors[i], line_to_write, strlen(array_list[j]));
                            free(line_to_write);
                        }*/
                    }
                    else
                    {
                        char message[] = "No arrays currently running, or cmc not yet polled. Please try again later...\n";
                        /* TODO make this auto-refreshing as well.*/
                        r = write(file_descriptors[i], message, sizeof(message));
                    }
                    shutdown(file_descriptors[i], SHUT_RDWR);
                    close(file_descriptors[i]);
                    file_descriptors[i] = -1;
                    file_descriptors_want_data[i] = 0;
                }
            }
        }
    }

    /* cleanup */
    destroy_katcl(l, 1);

    return 0;
}

