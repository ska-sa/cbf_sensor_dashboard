#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <syslog.h>

#include "message.h"
#include "queue.h"

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
        if (this_queue->message_queue != NULL)
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
        //TODO figure out how to make this error message a bit more useful. Queue doesn't know who its parents are.
        syslog(LOG_ERR, "Attempted to push %s onto a NULL queue.", composed_message);
        free(composed_message);
        return -1;
    }
    if (new_message == NULL)
    {
        syslog(LOG_ERR, "Attempted to push NULL message onto a queue.");
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
        syslog(LOG_ERR, "Couldn't reallocate message queue.");
        return -3;
    }
}


struct message *queue_pop(struct queue *this_queue)
{
    if (this_queue == NULL)
    {
        syslog(LOG_ERR, "Attempted to pop NULL queue.");
        return NULL;
    }
    if (this_queue->queue_length == 0)
    {
        syslog(LOG_ERR, "Attempted to pop zero-length queue.");
        return NULL;
    }

    //copy first word in queue over.
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
            this_queue->message_queue = NULL;
            this_queue->queue_length = 0;
        }
        else
        {
            syslog(LOG_ERR, "Unable to realloc() memory for newly-shortened queue!");
        }
    }
    return front_message;
}


size_t queue_sizeof(struct queue *this_queue)
{
    return this_queue->queue_length;
}
