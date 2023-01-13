/* queue.h: Concurrent Queue of Requests */

#ifndef QUEUE_H
#define QUEUE_H

#include "mq/request.h"
#include "mq/thread.h"

/* Structures */

typedef struct Queue Queue;
struct Queue {
    Request *head;
    Request *tail;
    size_t   size;

    // Add any necessary thread and synchronization primitives
    Mutex lock;
    Cond cv;
};

/* Functions */

Queue *	    queue_create();
void        queue_delete(Queue *q);

void	    queue_push(Queue *q, Request *r);
Request *   queue_pop(Queue *q);

#endif