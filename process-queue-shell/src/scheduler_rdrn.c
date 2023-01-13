/* scheduler_rdrn.c: PQSH Round Robin Scheduler */

#include "pqsh/macros.h"
#include "pqsh/scheduler.h"

#include <assert.h>

/**
 * Schedule next process using round robin policy:
 *
 *  1. If there no waiting processes, then do nothing.
 *
 *  2. Move one process from front of running queue and place in back of
 *  waiting queue.
 *
 *  3. Move one process from front of waiting queue and place in back of
 *  running queue.
 *
 * @param   s	    Scheduler structure
 **/
void scheduler_rdrn(Scheduler *s) {
    // Check if we need to pause a running process
    if(s->running.size == s->cores) {
        // Pause and remove running process from running queue
        Process* paused = queue_pop(&s->running);
        process_pause(paused);
        queue_push(&s->waiting, paused);
    }
    // Start new processes on all available cores
    while(s->running.size < s->cores && s->waiting.size != 0) {
        Process* run_next = queue_pop(&s->waiting);
        
        if(run_next->pid == 0) {
            process_start(run_next);
        }
        else {
            process_resume(run_next);
        }
        queue_push(&s->running, run_next);
    }
}