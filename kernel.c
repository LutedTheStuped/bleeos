/* BleeOS 32-bit C kernel: banner + POSIX-style shell.
 * Entered from the boot menu as kernel_main(&info). Returns on `exit`. */
#include "drivers.h"
#include "boot.h"
#include "shell.h"
#include "ata.h"
#include "usb.h"
#include "uhci.h"
#include "users.h"

static int has_opt(const char *cmdline, const char *opt) {
    int ol = 0;
    while (opt[ol]) ol++;
    for (const char *p = cmdline; *p; p++) {
        if (*p == ' ') continue;
        int i = 0;
        while (p[i] && p[i] != ' ' && opt[i] && p[i] == opt[i]) i++;
        if (i == ol && (p[i] == 0 || p[i] == ' ')) return 1;
        while (*p && *p != ' ') p++;
        if (!*p) break;
    }
    return 0;
}

void kernel_main(const boot_info_t *info) {
    int have = info && info->magic == BOOT_MAGIC;
    const char *cmdline = have ? info->cmdline : "";
    int verbose = has_opt(cmdline, "verbose");
    /* boot arg: mirror the serial kernel log onto the VGA text screen */
    int mirror  = has_opt(cmdline, "klog=vga") || has_opt(cmdline, "console=vga");

    vga_clear();
    klog_init();                     /* no-op if the boot menu already did it */
    klog_level(verbose ? KLOG_DEBUG : KLOG_INFO);
    klog_mirror(mirror);

    klogf(KLOG_INFO, "kernel: BleeOS 0.3 entered at kernel_main (stage2 @0x7E00)");
    klogf(KLOG_INFO, "cpu: 32-bit protected mode, flat GDT, interrupts off");
    klogf(KLOG_INFO, "mem: stack @0x90000, no paging/heap yet, ramfs in low memory");
    klogf(KLOG_INFO, "cmdline: \"%s\"", cmdline[0] ? cmdline : "(empty)");
    if (have) {
        klogf(KLOG_INFO, "boot: menu entry %u, drive 0x%02X (%s), boot time %u s",
              info->selected, info->boot_drive,
              info->boot_drive & 0x80 ? "hard disk" : "removable", info->boot_sec);
    } else {
        klogf(KLOG_WARN, "boot: no boot_info handed over (kernel entered directly)");
    }
    klogf(KLOG_INFO, "console: VGA text 80x25 @0xB8000, hardware cursor");
    klogf(KLOG_INFO, "console: log -> COM1 0x3F8 38400 8N1%s",
          mirror ? " + VGA text mirror (boot arg klog=vga)"
                 : " (VGA screen left to the menu/shell)");
    klogf(KLOG_INFO, "console: level %s%s",
          verbose ? "DEBUG" : "INFO", verbose ? " (`verbose`)" : "");
    klogf(KLOG_INFO, "input: PS/2 keyboard polled, arrows/Home/End/Del supported");
    klogf(KLOG_INFO, "fs: ramfs mounted on / (/motd /version /etc/hostname)");
    if (mirror)
        klogf(KLOG_INFO, "console: `klog=vga` set - the log is mirrored here too");

    vga_setcolor(0x0B);
    klog("==============================\n"
         "  BleeOS 0.3 - 32-bit mode\n"
         "  boot menu + C kernel shell\n"
         "==============================\n");
    vga_setcolor(0x07);
    if (have) {
        vga_print("cmdline: ");
        vga_print(info->cmdline);
        vga_print("\n");
    }
    if (verbose) {
        char num[12];
        int i = 0;
        unsigned sel = info ? info->selected : 0;
        if (sel == 0) num[0] = '0', num[1] = 0;
        else { while (sel) { num[i++] = (char)('0' + sel % 10); sel /= 10; } num[i] = 0; }
        vga_print("verbose: menu entry ");
        vga_print(num);
        vga_print(", protected mode, stage2 @0x7E00, stack @0x90000\n");
        vga_print("verbose: VGA 80x25, PS/2 poll, PIT/RTC, ramfs mounted on /\n");
        klogf(KLOG_DEBUG, "kernel: verbose boot, entry %s, ramfs mounted",
              num);
        klogf(KLOG_DEBUG, "pit: channel 0 free-running at %u Hz (uptime base)",
              1193182u);
        klogf(KLOG_DEBUG, "rtc: epoch %u s at menu start", info ? info->boot_sec : 0);
    }
    vga_print("Type `help` for commands, `exit` for boot menu.\n");
    {
        /* installed = booted from hard disk (BIOS DL 0x80+): the user
         * DB then persists on reserved HDD sectors across reboots */
        int installed = have && (info->boot_drive & 0x80);
        users_set_installed(installed);
        if (installed) vga_print("installed on HDD: users persist.\n");
        klogf(installed ? KLOG_INFO : KLOG_DEBUG, "users: %s",
              installed ? "installed mode - user DB on HDD LBA 256..260"
                        : "live media - user DB kept in ramfs");
    }
    if (ata_init() == 0) {
        ata_dev_t d;
        char num[16];
        if (ata_info(0, &d) == 0) {
            klogf(KLOG_INFO, "ata: primary master ready - `install` targets it");
            vga_print("ata: primary master ");
            vga_print(d.model);
            vga_print(" (");
            vga_print(utoa10(d.sectors / 2048, num));
            vga_print(" MB) - `install` writes BleeOS to it\n");
        }
    }
    {
        /* USB stub: detector only, PS/2 stays live */
        usb_scan();
        if (uhci_present()) {
            char num[12];
            int c0 = uhci_connected(0), c1 = uhci_connected(1);
            klogf(KLOG_INFO, "usb: UHCI @0x%04X port0=%s port1=%s (detector only)",
                  uhci_iobase(), c0 > 0 ? "dev" : "empty", c1 > 0 ? "dev" : "empty");
            vga_print("usb: UHCI detected @");
            vga_print(utoa10(uhci_iobase(), num));
            vga_print(" p0=");
            vga_print(c0 > 0 ? "dev" : "empty");
            vga_print(" p1=");
            vga_print(c1 > 0 ? "dev" : "empty");
            vga_print(" (stub, PS/2 active)\n");
        }
    }
    klogf(KLOG_INFO, "shell: starting (uptime %u.%03u s)",
          klog_uptime_sec(), klog_uptime_msec());
    shell_run(have ? info->boot_sec : 0, verbose);
    klogf(KLOG_INFO, "shell: returned to the boot menu");
    vga_print("\nBack to boot menu...\n");
    sleep_ms(600);
}
