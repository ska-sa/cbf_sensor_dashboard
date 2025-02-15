/*
 * CBF sensor dashboard
 * Author: James Smith
 *
 * Simple program to display an html-dashboard monitoring the state of the sensors in the correlator.
 *
 */

#include <stdio.h>
#include <syslog.h>
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
#include <time.h>
#include <katcp.h>
#include <katcl.h>
#include <netc.h>
#include <execinfo.h>

#include "cmc_server.h"
#include "cmc_aggregator.h"
#include "message.h"
#include "tokenise.h"
#include "utils.h"
#include "web.h"

#define BUF_SIZE 1024
#define CMC_CONFIG_FILE "/etc/cbf_sensor_dashboard/cmc_list.conf"
/* This is handy for keeping track of the number of file descriptors. */
#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))


/********   SECTION    ***********
 * set up command line options and arguments
 *********************************/
const char *argp_program_version =
  "cbf_sensor_dashboard 1.3";
const char *argp_program_bug_address =
  "<jsmith@ska.ac.za>";
static char doc[] =
  "CBF Sensor Dashboard server";
static char args_doc[] = "LISTEN_PORT";

static struct argp_option options[] = {
  {"verbose",  'v', "VERBOS_LVL",      0,  "Level of verbosity for the output logs, according to rsyslog's standard levels." },
  { 0 }
};

struct arguments
{
  char *args[1];                /* only listen_port at the moment */
  int verbose;
};

static error_t parse_opt (int key, char *arg, struct argp_state *state)
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


/********   SECTION    ***********
 * Signal handler to handle sane exiting of the program.
 *********************************/

static volatile sig_atomic_t stop = 0;
static void handler(int signo)
{
    switch (signo) {
        case SIGINT:
        case SIGTERM:
            syslog(LOG_INFO, "Caught signal to terminate, exiting cleanly...");
            stop = 1;
            break;
        case SIGSEGV:
            ; //Need this because ... a label can only be part of a statement and a declaration isn't a statement?
            void *array[10];
            int size;
            size = backtrace(array, 10);
            fprintf(stderr, "Error: signal %d:\n", signo);
            backtrace_symbols_fd(array, size, STDERR_FILENO);
            exit(1);
            break;
        case SIGPIPE:
            ; //ignore, nothing to see here...
        default:
            ;
    }
}


/********   SECTION    ***********
 * main()
 *********************************/

int main(int argc, char **argv)
{

    openlog(NULL, 0, LOG_DAEMON);
    syslog(LOG_INFO, "Starting...");

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

    /*r = sigaction(SIGSEGV, &act, 0);
    if (r)
    {
        perror("sigaction (sigsegv)");
        return -1;
    }*/

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
    setlogmask(LOG_UPTO(arguments.verbose));

    /********   SECTION    ***********
     * read list of cmcs from the config file, populate array of structs
     *********************************/

    FILE *cmc_config = fopen(CMC_CONFIG_FILE, "r");
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
                    perror("New CMC server allocation"); //Not sure if perror is appropriate here.
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
     * setup listening on websocket
     *********************************/

    uint16_t listening_port = (uint16_t) atoi(arguments.args[0]);
    int server_fd = listen_on_socket(listening_port);
    if (server_fd == -1)
    {
        syslog(LOG_CRIT, "Unable to create socket on port %u!\n", listening_port);
        return -1;
    }

    /********   SECTION    ***********
     * setup web client management
     *********************************/

    struct web_client **client_list = NULL;
    size_t num_web_clients = 0;

    /********   SECTION    ***********
     * select() loop
     *********************************/

    time_t last_array_list_poll = time(0);
    struct timespec timeout;
    timeout.tv_sec = 1;
    timeout.tv_nsec = 0;

    while (!stop)
    {
        //printf("new select() loop\n");
        int nfds = 0;
        fd_set rd, wr;

        FD_ZERO(&rd);
        FD_ZERO(&wr);

        FD_SET(server_fd, &rd);
        nfds = max(nfds, server_fd);

        if ((time(0) - last_array_list_poll) >= 60) //check for a change
        {
            for (i = 0; i < num_cmcs; i++)
                cmc_server_poll_array_list(cmc_list[i]);
            last_array_list_poll = time(0);
        }

        for (i = 0; i < num_cmcs; i++)
        {
            cmc_server_setup_katcp_writes(cmc_list[i]);
            cmc_server_set_fds(cmc_list[i], &rd, &wr, &nfds);
        }

        for (i = 0; i < num_web_clients; i++)
        {
            web_client_set_fds(client_list[i], &rd, &wr, &nfds);
        }
        
        r = pselect(nfds + 1, &rd, &wr, NULL, &timeout, &orig_mask);

        if (r == -1 && errno == EINTR)
            continue; // Just interrupted, not a problem.

        if (r < 0)
        {
            syslog(LOG_CRIT, "pselect() error! Must exit now.");
            perror("pselect()");
            exit(EXIT_FAILURE);
        }

        if (r == 0) //timeout
        {
            for (i = 0; i < num_cmcs; i++)
            {
                //This will only do something if it has disconnected.
                cmc_server_try_reconnect(cmc_list[i]);
            }
        }

        if (r > 0) 
        {
            //handle reads and writes from the CMC servers, to let them update anything that they need to.
            //This will include the underlying arrays, which are on separate FDs (and ports) but at the same IP address.
            for (i = 0; i < num_cmcs; i++)
            {
                cmc_server_socket_read_write(cmc_list[i], &rd, &wr);
                cmc_server_handle_received_katcl_lines(cmc_list[i]);
            }

            //CMCs have been polled for their arrays at this point, set up the aggregator
            struct cmc_aggregator* cmc_agg = cmc_aggregator_create(cmc_list, num_cmcs);
            
            //Check with the web server to see if a new client wants to connect.
            if (FD_ISSET(server_fd, &rd))
            {
                unsigned int len;
                struct sockaddr_in client_address;
                memset(&client_address, 0, len = sizeof(client_address));
                r = accept(server_fd, (struct sockaddr *) &client_address, &len);
                if (r == -1)
                {
                    perror("accept()");
                }
                else
                {
                    //syslog(LOG_DEBUG, "Connection from %s:%u (FD %d)\n", inet_ntoa(client_address.sin_addr), client_address.sin_port, r);
                    struct web_client **temp = realloc(client_list, sizeof(*client_list)*(num_web_clients + 1));
                    if (temp)
                    {
                        client_list = temp;
                        client_list[num_web_clients] = web_client_create(r);
                        num_web_clients++;
                    }
                    else
                    {
                        shutdown(r, SHUT_RDWR);
                        close(r);
                        perror("realloc");
                        syslog(LOG_ERR, "Unable to allocate memory for another web client.\n");
                    }
                }
            }

            //Handle the web clients that have sent us stuff.
            for (i = 0; i < num_web_clients; i++)
            {
                r = web_client_socket_read(client_list[i], &rd);
                
                if (r < 0)
                {
                    web_client_destroy(client_list[i]);
                    if (num_web_clients == 1)
                    {
                        free(client_list);
                        client_list = NULL;
                        num_web_clients = 0;
                    }
                    else
                    {
                        memmove(&client_list[i], &client_list[i+1], sizeof(*client_list)*(num_web_clients - i - 1));
                        client_list = realloc(client_list, sizeof(*client_list)*(num_web_clients - 1));
                        num_web_clients--;
                        i--;
                    }
                }
                else
                {
                    //TODO handle requests.
                    web_client_handle_requests(client_list[i], cmc_list, num_cmcs, cmc_agg);
                }
            }
            
            cmc_aggregator_destroy(cmc_agg);

           //Handle web clients that we want to write to.
           //Looping through list a second time because number may have changed, some of them may have disconnected.
           //Prevents segfaults.
           for (i = 0; i < num_web_clients; i++)
           {
               r = web_client_socket_write(client_list[i], &wr);

                if (r < 0)
                {
                    web_client_destroy(client_list[i]);
                    if (num_web_clients == 1)
                    {
                        free(client_list);
                        client_list = NULL;
                        num_web_clients = 0;
                    }
                    else
                    {
                        memmove(&client_list[i], &client_list[i+1], sizeof(*client_list)*(num_web_clients - i - 1));
                        struct web_client **temp = realloc(client_list, sizeof(*client_list)*(num_web_clients - 1));
                        if (temp)
                        {
                            client_list = temp;
                            num_web_clients--;
                            i--;
                        }
                    }
                }
            }
        }
    }

    syslog(LOG_INFO, "Exited select loop.");
    /********   SECTION    ***********
     * cleanup
     *********************************/
    for (i = 0; i < num_cmcs; i++)
    {
        cmc_server_destroy(cmc_list[i]);
    }
    free(cmc_list);
    cmc_list = NULL;
    syslog(LOG_INFO, "Cleanup complete.");

    closelog();

    return 0;
}
