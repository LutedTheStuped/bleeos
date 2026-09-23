/* USB stub: detect only. */
#include "usb.h"
#include "uhci.h"
#include "drivers.h"

int usb_scan(void) {
    if (uhci_init()) return 0;
    return 0;   /* stub: never enumerates */
}

int usb_ndev(void) { return 0; }

void usb_debug_probe(void) {
    vga_print("usb probe: unimplemented (stub detector only)\n");
}
