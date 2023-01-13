/* scheduler_fifo.c: PQSH FIFO Scheduler */

#include "pqsh/macros.h"
#include "pqsh/scheduler.h"

#include <assert.h>

/**
 * Schedule next process using fifo policy:
 *
 *  As long as we have less running processes than our number of CPUS and
 *  there are waiting processes, then we should move a process from the
 *  waiting queue to the running queue.
 *
 * @param   s	    Scheduler structure
 */
void scheduler_fifo(Scheduler *s) {
    /* Implement FIFO Policy */
    while(s->cores > s->running.size && s->waiting.size > 0) {
        Process* up_next = queue_pop(&s->waiting);
        process_start(up_next);
        queue_push(&s->running, up_next);
    }
}