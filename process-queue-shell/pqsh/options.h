/* options.h: PQSH Options */

#ifndef PQSH_OPTIONS_H
#define PQSH_OPTIONS_H

#include "pqsh/scheduler.h"

#include <stdbool.h>

bool parse_command_line_options(int argc, char *argv[], Scheduler *s);

#endif