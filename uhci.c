/* UHCI stub detector: find the controller, read PORTSC.
 * Deliberately does not touch USBCMD (no reset, no schedule). */
#include "uhci.h"

/* PORTSC */
#define PS_CONN 0x0001

static u16 iobase;
static int present;

static u32 pci_cfg(u8 slot, u8 off) {
    outl(0xCF8, 0x80000000u | ((u32)slot << 11) | (off & 0xFC));
    return inl(0xCFC);
}

int uhci_present(void) { return present; }
u16 uhci_iobase(void) { return iobase; }
int uhci_nports(void) { return present ? 2 : 0; }

int uhci_init(void) {
    if (present) return 0;
    /* PCI: class 0x0C03, prog-if 0x00 = UHCI */
    for (int slot = 0; slot < 32; slot++) {
        u32 id = pci_cfg((u8)slot, 0);
        if (id == 0xFFFFFFFFu || id == 0) continue;
        if (pci_cfg((u8)slot, 8) >> 16 != 0x0C03) continue;
        if (((pci_cfg((u8)slot, 8) >> 8) & 0xFF) != 0x00) continue;
        u32 bar = pci_cfg((u8)slot, 0x20);
        if (!(bar & 1)) continue;   /* BAR4 must be I/O */
        iobase = (u16)(bar & ~0x1Fu);
        if (!iobase) continue;
        present = 1;
        break;
    }
    return present ? 0 : -1;
}

int uhci_connected(int p) {
    u16 ps;
    if (!present || p < 0 || p > 1) return -1;
    ps = inw((u16)(iobase + 0x10 + p * 2));
    return (ps & PS_CONN) ? 1 : 0;
}
