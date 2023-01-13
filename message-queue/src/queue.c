/* queue.c: Concurrent Queue of Requests */

#include "mq/queue.h"

/**
 * Create queue structure.
 * @return  Newly allocated queue structure.
 */
Queue* queue_create() {
    Queue* q = calloc(1, sizeof(Queue));

    if(q) {
        q->head = NULL;
        q->tail = NULL;
        q->size = 0;
        mutex_init(&q->lock, NULL);
        cond_init(&q->cv, NULL);
    }
    return q;
}

/**
 * Delete queue structure.
 * @param   q       Queue structure.
 */
void queue_delete(Queue *q) {
    if(q) {
        while(q->size > 0) {
            Request* removed = queue_pop(q);
            request_delete(removed);
        }
        free(q);
    }
}

/**
 * Push request to the back of queue.
 * @param   q       Queue structure.
 * @param   r       Request structure.
 */
void queue_push(Queue *q, Request *r) {
    // Grab the lock 
    mutex_lock(&q->lock);
    
    if(!q->head) {
        q->head = r;
        q->tail = r;
    }
    else {
        q->tail->next = r;
        q->tail = r;
    }
    q->size++;
    // Try to wake sleeping pop thread
    cond_signal(&q->cv);
    // Unlock
    mutex_unlock(&q->lock);
}

/**
 * Pop request to the front of queue (block until there is something to return).
 * @param   q       Queue structure.
 * @return  Request structure.
 */
Request * queue_pop(Queue *q) {
    mutex_lock(&q->lock);
    while(q->size == 0) {
        cond_wait(&q->cv, &q->lock);
    }
    
    Request* popped = q->head;
    if(q->head == q->tail) {
        q->tail = NULL;
    }
    q->head = q->head->next;
    q->size--;

    mutex_unlock(&q->lock);
    return popped;
}