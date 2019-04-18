#ifndef _ARRAY_HANDLING_H_
#define _ARRAY_HANDLING_H_

#include <katcp.h>
#include <katcl.h>

enum array_state {
    REQUEST_FUNCTIONAL_MAPPING,
    RECEIVE_FUNCTIONAL_MAPPING,
    RECEIVE_SENSOR_FHOST_DEVICE_STATUS_RESPONSE,
    MONITOR_SENSORS
};

struct fhost {
    char hostname[7]; /* skarab serial numbers are six digits long, plus the terminal null. */
    int host_number;
    char device_status[8]; /*nominal warn error*/
    int netw_rx;
    int spead_rx;
    int netw_reor;
    int dig;
    int sync;
    int cd;
    int pfb;
    int quant;
    int ct;
    int spead_tx;
    int netw_tx;
};

struct xhost {
    char hostname[7];
    int host_number;
    int device_status;
    int netw_rx;
    int netw_reor;
    int miss_pkt;
    int spead_rx;
    int bram_reord;
    int vacc;
    int spead_tx;
    int netw_tx;
};

struct cmc_array {
    char *name;
    int monitor_port;
    char* multicast_groups;
    int number_of_antennas;
    int monitor_socket_fd;
    struct katcl_line *l;
    enum array_state state;
    int host_counter;
    char *current_sensor_name;
    struct fhost **fhosts;
    struct xhost **xhosts;
};

char *read_full_katcp_line(struct katcl_line *l);

struct cmc_array *create_array(char *array_name, int monitor_port, char* multicast_groups, char* cmc_address);
char *get_array_name(struct cmc_array *array); /* user must free the resulting char* */
void destroy_array(struct cmc_array *array);
int request_functional_mapping(struct cmc_array *array);
int accept_functional_mapping(struct cmc_array *array);
int request_sensor_fhost_device_status(struct cmc_array *array);
int receive_sensor_fhost_device_status_response(struct cmc_array *array);

void process_sensor_status(struct cmc_array *array);

struct fhost *create_fhost(char *hostname, int host_number);
void destroy_fhost(struct fhost *fhost);
struct xhost *create_xhost(char *hostname, int host_number);
void destroy_xhost(struct xhost *xhost);

int listen_on_socket(int listening_port);
#endif
