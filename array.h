#ifndef _ARRAY_H_
#define _ARRAY_H_

struct array;

struct array *array_create(char *new_array_name, size_t number_of_antennas);
void array_destroy(struct array *this_array);

#endif
