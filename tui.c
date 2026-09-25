/* Debian-like dialogs on VGA text: header bar, ASCII boxes, 0x70
 * highlight, full redraw per state change (cheap at human speed). */
#include "tui.h"

static void header(const char *title) {
    vga_setcolor(0x07);
    vga_clear();
    vga_setcolor(0x1F);
    for (int i = 0; i < 80; i++) vga_putc(' ');
    vga_write_at(0, 2, title, 0x1F);
    vga_setcolor(0x07);
    vga_putc('\n');
    vga_putc('\n');
}

static void body_lines(const char *body) {
    while (*body) {
        if (*body == '\n') { vga_putc('\n'); body++; continue; }
        vga_putc(*body++);
    }
    vga_putc('\n');
}

int tui_menu(const char *title, const char *body, const char **items, int n) {
    int sel = 0;
    if (n <= 0) return -1;
    for (;;) {
        int i;
        header(title);
        if (body) { body_lines(body); vga_putc('\n'); }
        for (i = 0; i < n; i++) {
            vga_print("   ");
            if (i == sel) {
                int k;
                vga_setcolor(0x70);
                vga_print(" > ");
                vga_print(items[i]);
                k = 0;
                while (items[i][k]) k++;
                for (; k < 40; k++) vga_putc(' ');
                vga_setcolor(0x07);
            } else {
                vga_print("   ");
                vga_print(items[i]);
            }
            vga_putc('\n');
        }
        vga_print("\n   Up/Down select, Enter confirm, Esc back\n");
        int k = kbd_getkey();
        if (k == 27) return -1;
        if (k == '\n') return sel;
        if (k == KEY_UP || k == 'k') sel = (sel + n - 1) % n;
        else if (k == KEY_DOWN || k == 'j') sel = (sel + 1) % n;
        else if (k >= '1' && k <= '0' + n) return k - '1';
    }
}

int tui_input(const char *title, const char *prompt, const char *def,
              char *buf, u32 cap) {
    int r;
    header(title);
    if (prompt) { body_lines(prompt); vga_putc('\n'); }
    if (def) {
        vga_print("   [");
        vga_print(def);
        vga_print("] (empty keeps)\n\n");
    }
    vga_print("   > ");
    r = kbd_readline(buf, cap);
    if (r >= 0 && buf[0] == 0 && def) {
        u32 i = 0;
        while (def[i] && i + 1 < cap) { buf[i] = def[i]; i++; }
        buf[i] = 0;
        r = (int)i;
    }
    return r;
}

void tui_msg(const char *title, const char *msg) {
    header(title);
    if (msg) body_lines(msg);
    vga_print("\n   Press any key to continue\n");
    kbd_getkey();
}

static const char *prog_label;
static char prog_title[48];

void tui_progress(const char *title, const char *label) {
    u32 i = 0;
    prog_label = label;
    while (title[i] && i < sizeof(prog_title) - 1) {
        prog_title[i] = title[i];
        i++;
    }
    prog_title[i] = 0;
    tui_progress_update(0);
}

void tui_progress_update(int pct) {
    int i, fill;
    if (pct < 0) pct = 0;
    if (pct > 100) pct = 100;
    vga_setcolor(0x07);
    vga_clear();
    vga_setcolor(0x1F);
    for (i = 0; i < 80; i++) vga_putc(' ');
    vga_write_at(0, 2, prog_title, 0x1F);
    vga_setcolor(0x07);
    vga_putc('\n');
    vga_putc('\n');
    if (prog_label) { body_lines(prog_label); vga_putc('\n'); }
    vga_print("   [");
    fill = pct * 50 / 100;
    vga_setcolor(0x2F);
    for (i = 0; i < fill; i++) vga_putc('#');
    vga_setcolor(0x07);
    for (i = fill; i < 50; i++) vga_putc('-');
    vga_print("] ");
    {
        char b[12];
        vga_print(utoa10((u32)pct, b));
        vga_print("%\n");
    }
}
