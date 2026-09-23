/* BleeOS POSIX-style shell interface. */
#ifndef SHELL_H
#define SHELL_H

#include "drivers.h"

void shell_run(u32 boot_sec, int verbose);
const char *shell_hostname(void);
int shell_readline(char *buf);   /* line editor; len, -1 EOF, -2 cancel */

#endif
