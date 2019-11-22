#include <stdlib.h>

#include "cmc_aggregator.h"
#include "cmc_server.h"
#include "array.h"

//
/// A struct to aggregate and sort the lists of arrays from multiple cmc_server objects.
struct cmc_aggregator {
    /// A list of arrays from all the cmc_server objects, sorted by the number of antennas in them.
    struct array **array_list;
    /// The number of arrays in the list.
    size_t n_arrays;
};


/**
 * \fn      static int cmp_by_array_size(const void *p1, const void *p2)
 * \details Compare two arrays by their size, for use with qsort function. Includes pointers for appropriately casting.
 */
static int cmp_by_array_size(const void *p1, const void *p2)
{
    struct array *a1 = *(struct array **) p1;
    struct array *a2 = *(struct array **) p2;

    //a2 minus a1 will sort them from biggest to smallest.
    //TODO think about overflows. Numbers are likely to be small enough that this won't be an issue, but still.
    return array_get_size(a2) - array_get_size(a1);
}


/**
 * \fn      struct cmc_aggregator *cmc_aggregator_create(struct cmc_server **cmc_server_list, size_t n_cmc_servers)
 * \details Allocate memory for a cmc_aggregator object, and collect and sort all the arrays on the cmc_server lists.
 * \param   cmc_server_list A list of the cmc_server objects whose arrays we'd like to aggregate.
 * \param   n_cmc_servers The number of cmc_servers in the aforementioned list.
 * \return  A pointer to the newly-created aggregator.
 */
struct cmc_aggregator *cmc_aggregator_create(struct cmc_server **cmc_server_list, size_t n_cmc_servers)
{
    struct cmc_aggregator *new_cmc_aggregator = malloc(sizeof(*new_cmc_aggregator));
    
    new_cmc_aggregator->array_list = NULL;
    new_cmc_aggregator->n_arrays = 0;

    size_t i;
    for (i = 0; i < n_cmc_servers; i++)
    {
        //naughty, should do via a temp variable
        new_cmc_aggregator->array_list = realloc(new_cmc_aggregator->array_list, sizeof(*(new_cmc_aggregator->array_list))*(new_cmc_aggregator->n_arrays + cmc_server_get_n_arrays(cmc_server_list[i])));
        size_t j;
        for (j = 0; j < cmc_server_get_n_arrays(cmc_server_list[i]); j++)
        {
            new_cmc_aggregator->array_list[new_cmc_aggregator->n_arrays + j] = cmc_server_get_array(cmc_server_list[i], j);
        }
        new_cmc_aggregator->n_arrays += j;
    }
    qsort(new_cmc_aggregator->array_list, new_cmc_aggregator->n_arrays, sizeof(struct array *), cmp_by_array_size);

    return new_cmc_aggregator;
}


/**
 * \fn      void cmc_aggregator_destroy(struct cmc_aggregator *this_cmc_aggregator)
 * \details Free the memory allocated to the cmc_aggregator object.
 * \param   this_cmc_aggregator The cmc_aggregator in question.
 * \return  void
 */
void cmc_aggregator_destroy(struct cmc_aggregator *this_cmc_aggregator)
{
    if (this_cmc_aggregator != NULL)
    {
        free(this_cmc_aggregator->array_list); //MUSTN'T free the individual pointers, because they are prolly still in use.
        free(this_cmc_aggregator);
    }
}


/**
 * \fn      struct array *cmc_aggregator_get_array(struct cmc_aggregator *this_cmc_aggregator, size_t array_number)
 * \details Get a pointer to the array on the cmc_aggregator's array_list.
 * \param   this_cmc_aggregator A pointer to the cmc_aggregator in question.
 * \param   array_number The index of the array desired.
 * \return  A pointer to the requested array in the array_list, NULL if the index given is past the end of the list.
 */
struct array *cmc_aggregator_get_array(struct cmc_aggregator *this_cmc_aggregator, size_t array_number)
{
    if (array_number < this_cmc_aggregator->n_arrays)
        return this_cmc_aggregator->array_list[array_number];
    else
        return NULL;
}
