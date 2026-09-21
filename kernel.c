/* BleeOS 32-bit C kernel: banner + POSIX-style shell.
 * Entered from the boot menu as kernel_main(&info). Returns on `exit`. */
#include "drivers.h"
#include "boot.h"
#include "shell.h"

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
    int verbose = info && info->magic == BOOT_MAGIC &&
                  has_opt(info->cmdline, "verbose");

    vga_clear();
    vga_setcolor(0x0B);
    vga_print("==============================\n"
              "  BleeOS 0.3 - 32-bit mode\n"
              "  boot menu + C kernel shell\n"
              "==============================\n");
    vga_setcolor(0x07);
    if (info && info->magic == BOOT_MAGIC) {
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
    }
    vga_print("Type `help` for commands, `exit` for boot menu.\n");
    shell_run(info ? info->boot_sec : 0, verbose);
    vga_print("\nBack to boot menu...\n");
    sleep_ms(600);
}
