#ifndef _ARRAY_HANDLING_H_
#define _ARRAY_HANDLING_H_

#include <katcp.h>
#include <katcl.h>

enum array_state {
    REQUEST_FUNCTIONAL_MAPPING,
    RECEIVE_FUNCTIONAL_MAPPING,
    RECEIVE_SENSOR_SAMPLING_OK,
    MONITOR_SENSORS
};

struct fhost {
    char hostname[7]; /* skarab serial numbers are six digits long, plus the terminal null. */
    int host_number;
    char device_status[8]; /*nominal warn error*/
    char netw_rx[8];
    char spead_rx[8];
    char netw_reor[8];
    char dig[8];
    char sync[8];
    char cd[8];
    char pfb[8];
    char quant[8];
    char ct[8];
    char spead_tx[8];
    char netw_tx[8];
};

struct xhost {
    char hostname[7];
    int host_number;
    char netw_rx[8];
    char netw_reor[8];
    char miss_pkt[8];
    char spead_rx[8];
    char bram_reord[8];
    char xeng0_bram_reord[8];
    char xeng1_bram_reord[8];
    char xeng2_bram_reord[8];
    char xeng3_bram_reord[8];
    char vacc[8];
    char xeng0_vacc[8];
    char xeng1_vacc[8];
    char xeng2_vacc[8];
    char xeng3_vacc[8];
    char spead_tx[8];
    char netw_tx[8];
};

struct cmc_array {
    char *name;
    int monitor_port;
    char* multicast_groups;
    int number_of_antennas;
    int monitor_socket_fd;
    struct katcl_line *l;
    enum array_state state;
    struct fhost **fhosts;
    struct xhost **xhosts;
    char **sensor_names;
    int number_of_sensors;
    int current_sensor;
    char *current_sensor_name;

};

char *read_full_katcp_line(struct katcl_line *l);

struct cmc_array *create_array(char *array_name, int monitor_port, char* multicast_groups, char* cmc_address);
char *get_array_name(struct cmc_array *array); /* user must free the resulting char* */
void destroy_array(struct cmc_array *array);
int request_functional_mapping(struct cmc_array *array);
int accept_functional_mapping(struct cmc_array *array);

void request_next_sensor(struct cmc_array *array);
int receive_next_sensor_ok(struct cmc_array *array);

/*int request_sensor_fhost_device_status(struct cmc_array *array);
int receive_sensor_fhost_device_status_response(struct cmc_array *array);
*/
void process_sensor_status(struct cmc_array *array);

struct fhost *create_fhost(char *hostname, int host_number);
void destroy_fhost(struct fhost *fhost);
struct xhost *create_xhost(char *hostname, int host_number);
void destroy_xhost(struct xhost *xhost);

int listen_on_socket(int listening_port);
#endif
