#ifndef _ARRAY_HANDLING_H_
#define _ARRAY_HANDLING_H_

#include <katcp.h>
#include <katcl.h>

struct cmc_array {
    char *name;
    int monitor_port;
    char* multicast_groups;
    int monitor_socket_fd;
    struct katcl_line *l;
};

char *read_full_katcp_line(struct katcl_line *l);

struct cmc_array *create_array(char *array_name, int monitor_port, char* multicast_groups, char* cmc_address);
char *get_array_name(struct cmc_array *array); /* user must free the resulting char* */
void destroy_array(struct cmc_array *array);
//int request_sensor_list(struct cmc_array *array);

int listen_on_socket(int listening_port);
#endif
