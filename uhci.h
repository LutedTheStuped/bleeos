/* UHCI USB host controller: STUB detector only.
 * PCI-probes a UHCI controller and reports port connect state.
 * No resets, no schedules, no transfers: the BIOS-owned HC is left
 * untouched and PS/2 stays the input path (see mouse.c). */
#ifndef UHCI_H
#define UHCI_H

#include "drivers.h"

/* probe PCI for UHCI; 0 ok, -1 none present */
int uhci_init(void);
int uhci_present(void);
u16 uhci_iobase(void);
int uhci_nports(void);          /* 2 when present, else 0 */
/* port attach state: 1 device, 0 empty, -1 error */
int uhci_connected(int p);

#endif
