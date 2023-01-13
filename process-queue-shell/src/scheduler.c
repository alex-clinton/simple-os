/* scheduler.c: PQSH Scheduler */

#include "pqsh/macros.h"
#include "pqsh/scheduler.h"
#include "pqsh/timestamp.h"

#include <errno.h>
#include <sys/wait.h>

/**
 * Add new command to waiting queue.
 * @param   s	    Pointer to Scheduler structure.
 * @param   fs      File stream to write to.
 * @param   command Command string for new Process.
 **/
void scheduler_add(Scheduler *s, FILE *fs, const char *command) {
    Process *new_process = process_create(command);
    queue_push(&(s->waiting),new_process);
    fprintf(fs,"Added process \"%s\" to waiting queue.\n",command);
}

/**
 * Display status of queues in Scheduler.
 * @param   s	    Pointer to Scheduler structure.
 * @param   fs      File stream to write to.
 * @param   queue   Bitmask specifying which queues to display.
 **/
void scheduler_status(Scheduler *s, FILE *fs, int queue) {
    fprintf(fs, "Running = %4lu, Waiting = %4lu, Finished = %4lu, Turnaround = %05.2lf, Response = %05.2lf\n",
                s->running.size,s->waiting.size,s->finished.size,(s->total_turnaround_time/s->finished.size),
                (s->total_response_time/s->finished.size));

    /* Complement implementation. */
    if(queue && RUNNING && s->running.size) {
        fprintf(fs,"Running Queue:\n");
        queue_dump(&(s->running) ,fs);
        fprintf(fs,"\n");
    }
    if(queue && WAITING && s->waiting.size) {
        fprintf(fs,"Waiting Queue:\n");
        queue_dump(&(s->waiting) ,fs);
        fprintf(fs,"\n");
    }
    if(queue && FINISHED && s->finished.size) {
        fprintf(fs,"Finished Queue:\n");
        queue_dump(&(s->finished) ,fs);
        fprintf(fs,"\n");
    }
}

/**
 * Schedule next process using appropriate policy.
 * @param   s	    Pointer to Scheduler structure.
 **/
void scheduler_next(Scheduler *s) {
    /* Dispatch to appropriate scheduler function. */
    if(s->policy == FIFO_POLICY) {
        scheduler_fifo(s);
    }
    else {
        scheduler_rdrn(s);
    }
}

/**
 * Wait for any children and remove from queues and update metrics.
 * @param   s	    Pointer to Scheduler structure.
 **/
void scheduler_wait(Scheduler *s) {
    /* TODO: Wait for any children without blocking:
     *
     *  - Remove process from queues.
     *  - Update Process metrics.
     *  - Update Scheduler metrics.
     **/

    pid_t pid;
    while((pid = waitpid(-1, NULL, WNOHANG)) > 0) {
        // Remove process from running queue
        Process* found = queue_remove(&s->running, pid);

        found->end_time = timestamp();
        // Put process into finished queue
        queue_push(&s->finished, found);

        double turnaround_time = found->end_time - found->arrival_time;
        double response_time = found->start_time - found->arrival_time;

        s->total_turnaround_time += turnaround_time;
        s->total_response_time += response_time;
    }
}
