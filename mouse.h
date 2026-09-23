/* PS/2 mouse via ports 0x60/0x64 (polled, bounded waits). */
#ifndef MOUSE_H
#define MOUSE_H

#include "drivers.h"

int mouse_init(void);   /* 0 ok, -1 no mouse (sets 60Hz sample rate) */
int mouse_poll(int *dx, int *dy, int *btn);  /* 1 = packet done */
int mouse_pending(void);  /* unread mouse byte waiting (arrived mid-frame) */
void mouse_resync(void);  /* drop a stale partial packet */

#endif
