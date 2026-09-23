/* USB: stub layer over the UHCI detector. No enumeration, no
 * transfers; PS/2 stays the input path. */
#ifndef USB_H
#define USB_H

/* runs the detector; always 0 devices (stub) */
int usb_scan(void);
int usb_ndev(void);
void usb_debug_probe(void);  /* prints "unimplemented" note */

#endif
