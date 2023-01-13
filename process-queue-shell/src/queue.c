/* queue.c: PQSH Queue */

#include "pqsh/macros.h"
#include "pqsh/queue.h"

#include <assert.h>

/**
 * Push process to back of queue.
 * @param q     Pointer to Queue structure.
 **/
void queue_push(Queue *q, Process *p) {
    // Error checking
    if(!q || !p) {
        return;
    }
    // Check whether or not the process is the first item to be added
    if(!(q->tail)) {
        q->head = p;
    }
    else {
        q->tail->next = p;
    }
    q->tail = p;
    (q->size)++;
}

/**
 * Pop process from front of queue.
 * @param q     Pointer to Queue structure.
 * @return  Process from front of queue.
 **/
Process* queue_pop(Queue *q) {
    // Handle errors
    if(!q || !(q->head)) {
        return NULL;
    }

    // If size == 1, set tail to NULL
    if(q->size ==1) {
      q->tail = NULL;
    }
    // Update head
    Process* front = q->head;
    q->head = q->head->next;
    front->next = NULL;
    q->size--;
    return front;
}

/**
 * Remove and return process with specified pid.
 * @param q     Pointer to Queue structure.
 * @param pid   Pid of process to return.
 * @return  Process from Queue with specified pid.
 **/
Process*  queue_remove(Queue *q, pid_t pid) {
    // Error handling
    if(!q) {
        return NULL;
    }

    Process* curr = q->head;
    Process* prev = NULL;

    // Loop over queue elements
    while(curr) {
        // Desired pid found
        if(curr->pid == pid) {
            // Element at the start (avoids using prev)
            if(curr == q->head) {
                q->head = curr->next;
            }
            // Element at the end
            if(curr == q->tail) {
                q->tail = prev;
            }
            if(prev) {
                prev->next = curr->next;
            }

            q->size--;
            curr->next = NULL;
            break;
        }
        prev = curr;
        curr = curr->next;
    }
    return curr;
}

/**
 * Dump the contents of the Queue to the specified stream.
 * @param q     Queue structure.
 * @param fs    Output file stream.
 **/
void queue_dump(Queue *q, FILE *fs) {
    fprintf(fs, "%6s %-30s %-13s %-13s %-13s\n",
                "PID", "COMMAND", "ARRIVAL", "START", "END");
    /* Display information for each item in Queue. */
    Process* c = q->head;
    while(c) {
        fprintf(fs, "%6ld %-30s %13.2lf %13.2lf %13.2lf\n",
                (long)c->pid, c->command, c->arrival_time, c->start_time, c->end_time);
        c = c->next;
    }
}