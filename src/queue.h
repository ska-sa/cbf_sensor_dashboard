#ifndef _QUEUE_H_
#define _QUEUE_H_

#include "message.h"


/**
 * \file  queue.h
 * \brief The queue type stores a list of messages waiting to be sent.
 */


struct queue;

struct queue *queue_create();
void queue_destroy(struct queue *this_queue);

int queue_push(struct queue *this_queue, struct message *new_message);
struct message *queue_pop(struct queue *this_queue);
size_t queue_sizeof(struct queue *this_queue);

#endif
/*TODO need to change this so that the message itself becomes an array of strings, because there could be several.*/
