#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "message.h"
#include "queue.h"
#include "verbose.h"

struct queue {
    struct message **message_queue;
    size_t queue_length;
};


struct queue *queue_create()
{
    struct queue *new_queue = malloc(sizeof(*new_queue));
    if (new_queue != NULL)
    {
        new_queue->queue_length = 0;
        new_queue->message_queue = NULL; /*otherwise the kraken comes...*/
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
            message_destroy(this_queue->message_queue[i]);
        }
        free(this_queue->message_queue);
        this_queue->message_queue = NULL;
        free(this_queue);
        this_queue = NULL;
    }
}


int queue_push(struct queue *this_queue, struct message *new_message)
{
    if (this_queue == NULL)
    {
        char *composed_message = message_compose(new_message);
        verbose_message(ERROR, "Attempted to push %s onto a NULL queue.\n", composed_message);
        free(composed_message);
        return -1;
    }
    if (new_message == NULL)
    {
        verbose_message(ERROR, "Attempted to push NULL message onto a queue.\n");
        return -2;
    }
    struct message **temp = realloc(this_queue->message_queue, sizeof(*(this_queue->message_queue))*(this_queue->queue_length + 1));
    if (temp != NULL)
    {
        temp[this_queue->queue_length] = new_message;
        this_queue->message_queue = temp;
        this_queue->queue_length++;
        return 0;
    }
    else
    {
        perror("realloc");
        verbose_message(ERROR, "Couldn't reallocate message queue.\n");
        return -3;
    }
}


struct message *queue_pop(struct queue *this_queue)
{
    if (this_queue == NULL)
    {
        verbose_message(ERROR, "Attempted to pop NULL queue.\n");
        return NULL;
    }
    if (this_queue->queue_length == 0)
    {
        verbose_message(ERROR, "Attempted to pop zero-length queue.\n");
        return NULL;
    }

    struct message *front_message = message_create(message_get_type(this_queue->message_queue[0]));
    size_t i;
    for (i = 0; i < message_get_number_of_words(this_queue->message_queue[0]); i++)
    {
        message_add_word(front_message, message_see_word(this_queue->message_queue[0], i));
    }
    message_destroy(this_queue->message_queue[0]);
    memmove(&this_queue->message_queue[0],
            &this_queue->message_queue[1],
            sizeof(*(this_queue->message_queue))*(this_queue->queue_length - 1));
    struct message **temp = realloc(this_queue->message_queue, sizeof(*(this_queue->message_queue))*(this_queue->queue_length - 1));
    if (temp != NULL)
    {
        this_queue->message_queue = temp;
        this_queue->queue_length--;
    }
    else
    {
        if (this_queue->queue_length == 1)
        {
            verbose_message(INFO, "Queue length for queue 0x%08x now zero.\n", this_queue);
            this_queue->queue_length = 0;
        }
        else
        {
            verbose_message(ERROR, "Unable to realloc() memory for newly-shortened queue!\n");
        }
    }
    return front_message;
}


size_t queue_sizeof(struct queue *this_queue)
{
    return this_queue->queue_length;
}
