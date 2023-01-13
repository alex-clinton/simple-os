/* pqsh.c: Process Queue Shell */

#include "pqsh/macros.h"
#include "pqsh/options.h"
#include "pqsh/scheduler.h"
#include "pqsh/signal.h"

#include <errno.h>
#include <string.h>
#include <sys/time.h>

/* Global Variables */

Scheduler PQShellScheduler = {
    .policy    = FIFO_POLICY,
    .cores     = 1,
    .timeout   = 250000,
};

/* Help Message */

void help() {
    printf("Commands:\n");
    printf("  add    command    Add command to waiting queue.\n");
    printf("  status [queue]    Display status of specified queue (default is all).\n");
    printf("  help              Display help message.\n");
    printf("  exit|quit         Exit shell.\n");
}

/* Main Execution */

int main(int argc, char *argv[]) {
    Scheduler *s = &PQShellScheduler;

    /* Parse command line options */
    bool parsed = parse_command_line_options(argc, argv, &PQShellScheduler);
    if(!parsed) {
        exit(EXIT_FAILURE);
    }

    /* Register signal handlers */
    bool success = signal_register(SIGALRM, 0, sigalrm_handler);
    if(!success) {
        exit(EXIT_FAILURE);
    }

    /* Timer interrupt */
    struct itimerval interval = {
        .it_interval = { .tv_sec = 0, .tv_usec = s->timeout },
        .it_value    = { .tv_sec = 0, .tv_usec = s->timeout },
    };

    if(setitimer(ITIMER_REAL, &interval, NULL) < 0) {
        exit(EXIT_FAILURE);
    }

    /* Process shell comands */
    while (!feof(stdin)) {
        char command[BUFSIZ]  = "";
        printf("\nPQSH> ");

        while (!fgets(command, BUFSIZ, stdin) && !feof(stdin));

        // Prevent seg fault by catching last run after the while
        if(!strlen(command)) {
            break;
        }

        char* split_cmd = strtok(command," \n");
        /* TODO: Handle add and status commands */
        if(strcmp(command,"\n")) {
            if (streq(split_cmd, "help")) {
                help();
            }
            else if (streq(split_cmd, "exit") || streq(command, "quit")) {
                break;
            } 
            else if (streq(split_cmd, "status")) {
                split_cmd = strtok(NULL," ");

                if(split_cmd) {
                    printf("%s\n",split_cmd);
                }
                else {
                    scheduler_status(s,stdout,7);
                }
            } 
            else if (streq(split_cmd, "add")) {
                split_cmd = strtok(NULL,"\n");
                if(split_cmd) {
                    scheduler_add(s,stdout,split_cmd);
                }
                else {
                    printf("Unknown command: %s\n", command);
                }
            }
            else if (strlen(command)) {
                printf("Unknown command: %s\n", command);
            }
        }
    }
    return EXIT_SUCCESS;
}