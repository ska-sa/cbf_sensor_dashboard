/*
 * CBF sensor dashboard
 * Author: James Smith
 *
 * Simple program to display an html-dashboard monitoring the state of the sensors in the correlator.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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

#include "cmc_server.h"
#include "tokenise.h"

#define BUF_SIZE 1024

/* This is handy for keeping track of the number of file descriptors. */
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

static volatile sig_atomic_t stop = 0;
static void handler(int signo)
{
    //if (signo == SIGINT || signo == SIGTERM)
    if (signo == SIGINT)
    {
        fprintf(stderr, "Exiting cleanly...\n");
        stop = 1;
    }
}


int main()
{
    int r;
    size_t i;
    
    sigset_t mask;
    sigset_t orig_mask;
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = handler;
    r = sigaction(SIGINT, &act, 0);
    if (r)
    {
        perror("sigaction (sigint)");
        return -1;
    }

    /*
    r = sigaction(SIGTERM, &act, 0);
    if (r)
    {
        perror("sigaction (sigterm)");
        return -1;
    }
    */

    sigemptyset(&mask);
    //sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);

    r = sigprocmask(SIG_BLOCK, &mask, &orig_mask);
    if (r < 0)
    {
        perror("sigprocmask");
        return -1;
    }

    /********   SECTION    ***********
     * read list of cmcs from the config file, populate array of structs, connect to them and retrieve array list
     *********************************/

    FILE *cmc_config = fopen("cmc_list.conf", "r");
    if (cmc_config == NULL)
    {
        perror("Error (cmc_list.conf)");
        return -1;
    }

    struct cmc_server **cmc_list = NULL;
    size_t num_cmcs = 0;
    
    char buffer[BUF_SIZE];
    char *result;
    for (result = fgets(buffer, BUF_SIZE, cmc_config); result != NULL; result = fgets(buffer, BUF_SIZE, cmc_config))
    {
        char **tokens = NULL;
        size_t num_tokens;
        num_tokens = tokenise_string(buffer, ':', &tokens);
        if (num_tokens != 2)
        {
            fprintf(stderr, "Error in cmc_list.conf - expected 'hostname/ip:katcp-port', got %lu tokens instead: %s - ", num_tokens, buffer);
            for (i = 0; i < num_tokens; i++)
                fprintf(stderr, "%s, ", tokens[i]);
            fprintf(stderr, "\n");
        }
        else
        {
            struct cmc_server **temp = realloc(cmc_list, sizeof(*cmc_list)*(num_cmcs + 1));
            if (temp == NULL)
            {
                perror("CMC list allocation");
            }
            else
            {
                temp[num_cmcs] = cmc_server_create(tokens[0], (uint16_t) atoi(tokens[1]));
                if (temp[num_cmcs] == NULL)
                {
                    perror("New CMC server allocation");
                }
                else
                {
                    num_cmcs++;
                    cmc_list = temp;
                }
            }
        }
        for (i = 0; i < num_tokens; i++)
        {
            free(tokens[i]);
        }
        free(tokens);
    }

    /********   SECTION    ***********
     * select() loop
     *********************************/

    while (!stop)
    {
        //printf("new select() loop\n");
        int nfds = 0;
        fd_set rd, wr;

        FD_ZERO(&rd);
        FD_ZERO(&wr);

        for (i = 0; i < num_cmcs; i++)
        {
            FD_SET(cmc_list[i]->katcp_socket_fd, &rd);
            if (flushing_katcl(cmc_list[i]->katcl_line))
            {
                FD_SET(cmc_list[i]->katcp_socket_fd, &wr);
            }
            nfds = max(nfds, cmc_list[i]->katcp_socket_fd);
        }

        r = pselect(nfds + 1, &rd, &wr, NULL, NULL, &orig_mask);
        if (r < 0 && errno != EINTR)
        {
            perror("select");
            return -1;
        }

        for (i = 0; i < num_cmcs; i++)
        {
            if (FD_ISSET(cmc_list[i]->katcp_socket_fd, &rd))
            {
                r = read_katcl(cmc_list[i]->katcl_line);
                if (r)
                {
                    fprintf(stderr, "read from CMC%lu failed\n", i + 1);
                    perror("read_katcl()");
                    /*TODO some kind of error checking, what to do if the CMC doesn't connect.*/
                }
            }

            if (FD_ISSET(cmc_list[i]->katcp_socket_fd, &wr))
            {
                r = write_katcl(cmc_list[i]->katcl_line);
                if (r < 0)
                {
                    perror("write_katcl");
                    /*TODO some other kind of error checking.*/
                }
            }

            while (have_katcl(cmc_list[i]->katcl_line) > 0)
            {
                printf("From CMC%lu: %s %s %s %s %s\n", i + 1, \
                        arg_string_katcl(cmc_list[i]->katcl_line, 0), \
                        arg_string_katcl(cmc_list[i]->katcl_line, 1), \
                        arg_string_katcl(cmc_list[i]->katcl_line, 2), \
                        arg_string_katcl(cmc_list[i]->katcl_line, 3), \
                        arg_string_katcl(cmc_list[i]->katcl_line, 4)); 
            }
        }
    }

    printf("exited the loop.\n");
    /********   SECTION    ***********
     * cleanup
     *********************************/
    for (i = 0; i < num_cmcs; i++)
    {
        cmc_server_destroy(cmc_list[i]);
    }
    free(cmc_list);
    cmc_list = NULL;
    printf("cleanup complete.\n");

    return 0;
}

