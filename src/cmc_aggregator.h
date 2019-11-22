#ifndef _CMC_AGGREGATOR_H_
#define _CMC_AGGREGATOR_H_

#include "cmc_server.h"
#include "array.h"

struct cmc_aggregator;

struct cmc_aggregator *cmc_aggregator_create(struct cmc_server **cmc_server_list, size_t n_cmc_servers);
void cmc_aggregator_destroy(struct cmc_aggregator *this_cmc_aggregator);
struct array *cmc_aggregator_get_array(struct cmc_aggregator *this_cmc_aggregator, size_t array_number);

#endif
