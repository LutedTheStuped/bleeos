/* BleeOS 32-bit C kernel + shell (protected mode, freestanding).
 * No libc, no BIOS: direct VGA text buffer (0xB8000) + PS/2 keyboard polling.
 * Entry: kernel_main() called from kernel_entry.asm after the mode switch. */

typedef unsigned char  u8;
typedef unsigned short u16;
typedef unsigned int   u32;

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_BUF    ((volatile u16*)0xB8000)

/* ---------- port I/O ---------- */
static inline u8 inb(u16 port) {
    u8 v;
    __asm__ volatile ("inb %1, %0" : "=a"(v) : "Nd"(port));
    return v;
}
static inline void outb(u16 port, u8 v) {
    __asm__ volatile ("outb %0, %1" : : "a"(v), "Nd"(port));
}
static inline void io_wait(void) { outb(0x80, 0); }
static inline void cli(void) { __asm__ volatile ("cli"); }
static inline void hlt(void) { __asm__ volatile ("hlt"); }

/* ---------- tiny string utils ---------- */
static u32 strlen(const char* s) {
    u32 n = 0;
    while (s[n]) n++;
    return n;
}
static int strcmp(const char* a, const char* b) {
    while (*a && *a == *b) { a++; b++; }
    return (int)(u8)*a - (int)(u8)*b;
}
static int strncmp(const char* a, const char* b, u32 n) {
    for (u32 i = 0; i < n; i++) {
        if (a[i] != b[i] || a[i] == 0) return (int)(u8)a[i] - (int)(u8)b[i];
    }
    return 0;
}

/* ---------- VGA driver ---------- */
static u8 cursor_row, cursor_col, color = 0x07;

static void vga_cursor_update(void) {
    u16 pos = (u16)(cursor_row * VGA_WIDTH + cursor_col);
    outb(0x3D4, 0x0F); outb(0x3D5, (u8)(pos & 0xFF));
    outb(0x3D4, 0x0E); outb(0x3D5, (u8)((pos >> 8) & 0xFF));
}
static void vga_scroll(void) {
    for (u32 r = 0; r < VGA_HEIGHT - 1; r++)
        for (u32 c = 0; c < VGA_WIDTH; c++)
            VGA_BUF[r * VGA_WIDTH + c] = VGA_BUF[(r + 1) * VGA_WIDTH + c];
    for (u32 c = 0; c < VGA_WIDTH; c++)
        VGA_BUF[(VGA_HEIGHT - 1) * VGA_WIDTH + c] = (u16)(' ' | (color << 8));
}
static void vga_putc(char ch) {
    if (ch == '\n') {
        cursor_col = 0;
        if (++cursor_row >= VGA_HEIGHT) { vga_scroll(); cursor_row = VGA_HEIGHT - 1; }
    } else if (ch == '\b') {
        if (cursor_col > 0) {
            cursor_col--;
            VGA_BUF[cursor_row * VGA_WIDTH + cursor_col] = (u16)(' ' | (color << 8));
        }
    } else {
        VGA_BUF[cursor_row * VGA_WIDTH + cursor_col] = (u16)((u8)ch | (color << 8));
        if (++cursor_col >= VGA_WIDTH) {
            cursor_col = 0;
            if (++cursor_row >= VGA_HEIGHT) { vga_scroll(); cursor_row = VGA_HEIGHT - 1; }
        }
    }
    vga_cursor_update();
}
static void vga_print(const char* s) { while (*s) vga_putc(*s++); }
static void vga_clear(void) {
    for (u32 i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_BUF[i] = (u16)(' ' | (color << 8));
    cursor_row = 0; cursor_col = 0;
    vga_cursor_update();
}

/* ---------- PS/2 keyboard polling (scancode set 1) ---------- */
static const char sc_normal[58] = {
    0, 0, '1','2','3','4','5','6','7','8','9','0','-','=', '\b',
    '\t','q','w','e','r','t','y','u','i','o','p','[',']','\n',
    0,'a','s','d','f','g','h','j','k','l',';','\'','`',
    0,'\\','z','x','c','v','b','n','m',',','.','/',
    0,'*', 0,' ',
};
static const char sc_shift[58] = {
    0, 0, '!','@','#','$','%','^','&','*','(',')','_','+', '\b',
    '\t','Q','W','E','R','T','Y','U','I','O','P','{','}','\n',
    0,'A','S','D','F','G','H','J','K','L',':','"','~',
    0,'|','Z','X','C','V','B','N','M','<','>','?',
    0,'*', 0,' ',
};

/* blocking read of one translated char; handles shift/caps/backspace/enter */
static int shift_on, caps_on;

static char kbd_getc(void) {
    for (;;) {
        while (!(inb(0x64) & 0x01)) { /* wait for output buffer full */ }
        u8 sc = inb(0x60);

        if (sc == 0xE0) {               /* extended prefix: swallow next byte */
            while (!(inb(0x64) & 0x01)) { }
            (void)inb(0x60);
            continue;
        }
        if (sc & 0x80) {                /* key release */
            u8 mk = (u8)(sc & 0x7F);
            if (mk == 0x2A || mk == 0x36) shift_on = 0;
            continue;
        }
        if (sc == 0x2A || sc == 0x36) { shift_on = 1; continue; }
        if (sc == 0x3A) { caps_on = !caps_on; continue; }  /* caps lock toggle */
        if (sc >= 58) continue;

        char c = shift_on ? sc_shift[sc] : sc_normal[sc];
        if (!c) continue;
        /* apply caps to letters when shift not already flipping case */
        if (caps_on && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')))
            c = (char)(c ^ 0x20);
        return c;
    }
}

/* read a line with echo; returns length */
static u32 read_line(char* buf, u32 cap) {
    u32 len = 0;
    for (;;) {
        char c = kbd_getc();
        if (c == '\n') { vga_putc('\n'); buf[len] = 0; return len; }
        if (c == '\b') {
            if (len > 0) { len--; vga_putc('\b'); }
            continue;
        }
        if (c == '\t') c = ' ';
        if (len + 1 < cap) { buf[len++] = c; vga_putc(c); }
    }
}

/* ---------- shell ---------- */
static void cmd_help(void) {
    vga_print("Commands:\n"
              "  help         - show this help\n"
              "  echo <text>  - print text\n"
              "  clear        - clear screen\n"
              "  info / ver   - system info\n"
              "  reboot       - reboot machine\n"
              "  halt         - halt CPU\n");
}
static void cmd_info(void) {
    vga_print("BleeOS v0.2 | 32-bit protected mode | ASM entry + C kernel\n");
}

static void do_reboot(void) {
    vga_print("Rebooting...\n");
    /* keyboard controller reset */
    u8 good = 0x02;
    while (good & 0x02) good = inb(0x64);
    outb(0x64, 0xFE);
    io_wait();
    /* PCI reset fallback, then triple-fault fallback */
    outb(0xCF9, 0x0E);
    cli();
    __asm__ volatile ("lidt 0(%%eax)" : : "a"(0));
    __asm__ volatile ("int $3");
    for (;;) hlt();
}

static void shell(void) {
    static char line[256];
    for (;;) {
        color = 0x0A; vga_print("bleeos> "); color = 0x07;
        read_line(line, sizeof(line));

        if (line[0] == 0) continue;
        if (strcmp(line, "help") == 0) cmd_help();
        else if (strcmp(line, "clear") == 0) vga_clear();
        else if (strcmp(line, "info") == 0 || strcmp(line, "ver") == 0) cmd_info();
        else if (strcmp(line, "reboot") == 0) do_reboot();
        else if (strcmp(line, "halt") == 0) {
            vga_print("Halted. You can close QEMU.\n");
            cli();
            for (;;) hlt();
        }
        else if (strncmp(line, "echo", 4) == 0 &&
                 (line[4] == 0 || line[4] == ' ')) {
            if (line[4] == ' ') vga_print(line + 5);
            vga_putc('\n');
        }
        else vga_print("Unknown command. Type `help`.\n");
        (void)strlen; /* keep helper referenced in some builds */
    }
}

void kernel_main(void) {
    vga_clear();
    color = 0x0B;
    vga_print("==============================\n"
              "  BleeOS v0.2 - 32-bit mode\n"
              "  ASM entry + C kernel shell\n"
              "==============================\n");
    color = 0x07;
    vga_print("Type `help` for commands.\n");
    shell();
}
