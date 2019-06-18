#ifndef _QUEUE_H_
#define _QUEUE_H_

struct queue;

struct queue *queue_create();
void queue_destroy(struct queue *this_queue);

int queue_push(struct queue *this_queue, char *new_string);
char *queue_pop(struct queue *this_queue);
size_t queue_sizeof(struct queue *this_queue);

#endif
