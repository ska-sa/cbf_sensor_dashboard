/*
 * CBF sensor dashboard
 * Author: James Smith
 *
 * Simple program to display an html-dashboard monitoring the state of the sensors in the correlator.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <argp.h>
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
#include "message.h"
#include "tokenise.h"
#include "verbose.h"


const char *argp_program_version =
  "cbf-sensor-dashboard 0.2";
const char *argp_program_bug_address =
  "<jsmith@ska.ac.za>";
/* TODO Program documentation. */
static char doc[] =
  "CBF Sensor Dashboard server";
static char args_doc[] = "LISTEN_PORT";

static struct argp_option options[] = {
  {"verbose",  'v', "VERBOSITY_LEVEL",      0,  "Produce verbose output" },
  { 0 }
};

struct arguments
{
  char *args[1];                /* only listen_port at the moment */
  int verbose;
};


static error_t
parse_opt (int key, char *arg, struct argp_state *state)
{
  /* Get the input argument from argp_parse, which we
     know is a pointer to our arguments structure. */
  struct arguments *arguments = state->input;

  switch (key)
    {
    case 'v':
      arguments->verbose = atoi(arg);
      break;

    case ARGP_KEY_ARG:
      if (state->arg_num >= 1)
        /* Too many arguments. */
        argp_usage (state);

      arguments->args[state->arg_num] = arg;

      break;

    case ARGP_KEY_END:
      if (state->arg_num < 1)
        /* Not enough arguments. */
        argp_usage (state);
      break;

    default:
      return ARGP_ERR_UNKNOWN;
    }
  return 0;
}

/* Our argp parser. */
static struct argp argp = { options, parse_opt, args_doc, doc };

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


int main(int argc, char **argv)
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

    r = sigaction(SIGTERM, &act, 0);
    if (r)
    {
        perror("sigaction (sigterm)");
        return -1;
    }

    sigemptyset(&mask);
    sigaddset(&mask, SIGTERM);
    sigaddset(&mask, SIGINT);

    r = sigprocmask(SIG_BLOCK, &mask, &orig_mask);
    if (r < 0)
    {
        perror("sigprocmask");
        return -1;
    }

    struct arguments arguments;
    arguments.verbose = 0; //default
    argp_parse (&argp, argc, argv, 0, 0, &arguments);
    set_verbosity(arguments.verbose);    

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
    fclose(cmc_config);

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
            cmc_server_set_fds(cmc_list[i], &rd, &wr, &nfds);
            cmc_server_setup_katcp_writes(cmc_list[i]);
        }

        r = pselect(nfds + 1, &rd, &wr, NULL, NULL, &orig_mask);
        if (r > 0)
        {

            for (i = 0; i < num_cmcs; i++)
            {
                cmc_server_socket_read_write(cmc_list[i], &rd, &wr);
                cmc_server_handle_received_katcl_lines(cmc_list[i]);
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
