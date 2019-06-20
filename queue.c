#include <stdlib.h>
#include <string.h>

#include "queue.h"

struct queue {
    char **string_queue;
    size_t queue_length;
};


struct queue *queue_create()
{
    struct queue *new_queue = malloc(sizeof(*new_queue));
    if (new_queue != NULL)
    {
        new_queue->queue_length = 0;
        new_queue->string_queue= NULL; /*otherwise the kraken comes...*/
    }
    return new_queue;
}


void queue_destroy(struct queue *this_queue)
{
    if (this_queue != NULL)
    {
        size_t i;
        for (i = 0; i < this_queue->queue_length; i++)
        {
            free(this_queue->string_queue[i]);
        }
        free(this_queue);
    }
}


int queue_push(struct queue *this_queue, char *new_string)
{
    if (this_queue == NULL)
        return -1;
    if (new_string == NULL)
        return -2;
    char **temp = realloc(this_queue->string_queue, sizeof(*(this_queue->string_queue))*(this_queue->queue_length + 1));
    if (temp != NULL)
    {
        temp[this_queue->queue_length] = strdup(new_string);
        if (temp[this_queue->queue_length] == NULL)
            return -3;
        this_queue->string_queue = temp;
        this_queue->queue_length++;
        return 0;
    }
    else
        return -3;
}


char *queue_pop(struct queue *this_queue)
{
    if (this_queue == NULL || this_queue->queue_length == 0)
        return NULL;
    char *front_string = strdup(this_queue->string_queue[0]);
    memmove(&this_queue->string_queue[0], &this_queue->string_queue[1], sizeof(*(this_queue->string_queue))*(this_queue->queue_length - 1));
    this_queue->string_queue = realloc(this_queue->string_queue, sizeof(*(this_queue->string_queue))*(this_queue->queue_length - 1));
    this_queue->queue_length--;
    return front_string;
}


size_t queue_sizeof(struct queue *this_queue)
{
    return this_queue->queue_length;
}
