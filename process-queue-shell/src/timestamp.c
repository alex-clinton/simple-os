/* timestamp.c: PQSH Timestamp */

#include "pqsh/timestamp.h"

#include <time.h>
#include <sys/time.h>

/**
 * Return current timestamp as a double.
 *
 * Utilizes gettimeofday for precision (accounting for seconds and
 * microseconds) and falls back to time (only account for seconds) if that
 * fails.
 *
 * @return  Double representing the current time.
 **/
double timestamp() {
    struct timeval current_time;
    gettimeofday(&current_time, NULL);
    double the_time = current_time.tv_sec + ((double)current_time.tv_usec/1000000);
    return the_time;
}