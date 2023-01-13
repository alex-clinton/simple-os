/* process.c: PQSH Process */

#include "pqsh/macros.h"
#include "pqsh/process.h"
#include "pqsh/timestamp.h"

#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/**
 * Create new process structure given command.
 * @param   command     String with command to execute.
 * @return  Pointer to new process structure
 **/
Process *process_create(const char *command) {
    // Allocate space for new process
    Process* new_process = calloc(1, sizeof(Process));
    // Copy the command into the new process struct
    strcpy(new_process->command,command);
    // Update the arrival time
    new_process->arrival_time = timestamp();
    return new_process;
}

/**
 * Start process by forking and executing the command.
 * @param   p           Pointer to Process structure.
 * @return  Whether or not starting the process was successful
 **/
bool process_start(Process *p) {
    // Create child process
    pid_t pid = fork();
    p->pid = pid;

    // Fork failed
    if(p-pid < 0)
    {
        return false;
    }
    // Child
    else if(!pid) {
        char* argv[MAX_ARGUMENTS] = {0};
        int arg_count = 0;

        // Extract the arguments from the entire command and store pointers these new substrings
        for(char* token = strtok(p->command, " "); token; token =  strtok(NULL, " ")) {
            argv[arg_count++] = token;
        }

        execvp(argv[0], argv);
        // Exec only returns if it fails, if it does then we hit exit(1)
        exit(1);
    }
    // Parent
    else {
        // Record start time
        p->start_time = timestamp();
    }
    return true;
}

/**
 * Pause process by sending it the appropriate signal.
 * @param   p           Pointer to Process structure.
 * @return  Whether or not sending the signal was successful.
 **/
bool process_pause(Process *p) {
    int sig_sent = kill(p->pid, SIGSTOP);
    return sig_sent == 0;
}

/**
 * Resume process by sending it the appropriate signal.
 * @param   p           Pointer to Process structure.
 * @return  Whether or not sending the signal was successful.
 **/
bool process_resume(Process *p) {
    int sig_sent = kill(p->pid, SIGCONT);
    return sig_sent == 0;
}