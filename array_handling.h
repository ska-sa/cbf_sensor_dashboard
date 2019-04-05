#ifndef _ARRAY_HANDLING_H_
#define _ARRAY_HANDLING_H_

#include <katcp.h>
#include <katcl.h>

struct cmc_array {
    char *name;
    int monitor_port;
};

char *read_full_katcp_line(struct katcl_line *l);

struct cmc_array *create_array(char *array_name, int monitor_port);
char *get_array_name(struct cmc_array *array); /* user must free the resulting char* */
void destroy_array(struct cmc_array *array);

#endif
