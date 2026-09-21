/* PS/2 mouse via ports 0x60/0x64 (polled, bounded waits). */
#ifndef MOUSE_H
#define MOUSE_H

#include "drivers.h"

int mouse_init(void);   /* 0 ok, -1 no mouse */
int mouse_poll(int *dx, int *dy, int *btn);  /* 1 = packet done */

#endif
