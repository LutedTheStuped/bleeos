/* Ly-like login: big clock, hostname, user + password fields.
 * Keyboard only. Any password accepted (no user DB yet). */
#include "login.h"
#include "gfx.h"
#include "drivers.h"
#include "shell.h"
#include "wm.h"

#define C_BG     RGB(10, 14, 26)
#define C_BOX    RGB(24, 32, 54)
#define C_BORD   RGB(90, 140, 220)
#define C_TXT    RGB(230, 235, 245)
#define C_DIM    RGB(140, 155, 175)
#define C_FIELD  RGB(8, 10, 18)
#define C_ACCENT RGB(80, 170, 255)

static char wm_user_buf[32];

static void login_text(int x, int y, const char *s, u32 c) {
    gfx_text(x, y, s, c, GFX_TRANS);
}

static void draw_login(const char *user, const char *pass, int field,
                       const char *clock, const char *host) {
    int W = gfx_w(), H = gfx_h();
    gfx_noclip();
    gfx_fill(0, 0, W, H, C_BG);
    /* big clock + hostname, centered */
    int cw = gfx_textw(clock);
    gfx_text((W - cw) / 2, H / 2 - 130, clock, C_TXT, GFX_TRANS);
    int hw = gfx_textw(host);
    gfx_text((W - hw) / 2, H / 2 - 106, host, C_DIM, GFX_TRANS);
    /* dialog box */
    int bw = 320, bh = 132;
    int bx = (W - bw) / 2, by = (H - bh) / 2 - 10;
    gfx_fill(bx, by, bw, bh, C_BOX);
    gfx_rect(bx, by, bw, bh, C_BORD);
    login_text(bx + 20, by + 14, "login:", field == 0 ? C_ACCENT : C_DIM);
    gfx_fill(bx + 20, by + 30, bw - 40, 20, C_FIELD);
    gfx_rect(bx + 20, by + 30, bw - 40, 20, field == 0 ? C_ACCENT : C_BORD);
    gfx_text(bx + 26, by + 36, user, C_TXT, GFX_TRANS);
    if (field == 0)
        gfx_fill(bx + 26 + gfx_textw(user), by + 36, 7, 8, C_TXT);
    login_text(bx + 20, by + 58, "password:", field == 1 ? C_ACCENT : C_DIM);
    gfx_fill(bx + 20, by + 74, bw - 40, 20, C_FIELD);
    gfx_rect(bx + 20, by + 74, bw - 40, 20, field == 1 ? C_ACCENT : C_BORD);
    /* masked password */
    char masked[32];
    int i = 0;
    while (pass[i] && i < 31) { masked[i] = '*'; i++; }
    masked[i] = 0;
    gfx_text(bx + 26, by + 80, masked, C_TXT, GFX_TRANS);
    if (field == 1)
        gfx_fill(bx + 26 + gfx_textw(masked), by + 80, 7, 8, C_TXT);
    login_text(bx + 20, by + 104, "Enter: next/login   Esc: cancel", C_DIM);
}

int login_run(void) {
    char user[32], pass[32];
    int ulen = 0, plen = 0, field = 0;
    char lastsec = 0;
    user[0] = 0; pass[0] = 0;
    for (;;) {
        char timeb[20];
        rtc_format(timeb);
        char clock[6];
        clock[0] = timeb[11]; clock[1] = timeb[12]; clock[2] = timeb[13];
        clock[3] = timeb[14]; clock[4] = timeb[15]; clock[5] = 0;
        if (clock[5 - 1] != lastsec) {
            lastsec = clock[5 - 1];
            draw_login(user, pass, field, clock, shell_hostname());
        }
        int k = kbd_trykey();
        if (k == -1) { sleep_ms(50); continue; }
        if (k == 27) return 0;                       /* Esc: abort */
        if (k == '\t' || k == KEY_UP || k == KEY_DOWN) {
            field ^= 1;
            draw_login(user, pass, field, clock, shell_hostname());
            continue;
        }
        if (k == '\n') {
            if (field == 0) { field = 1; }
            else {
                int i = 0;
                while (user[i] && i < 31) { wm_user_buf[i] = user[i]; i++; }
                wm_user_buf[i] = 0;
                if (!wm_user_buf[0]) {
                    wm_user_buf[0] = 'g'; wm_user_buf[1] = 'u';
                    wm_user_buf[2] = 'e'; wm_user_buf[3] = 's';
                    wm_user_buf[4] = 't'; wm_user_buf[5] = 0;
                }
                wm_set_user(wm_user_buf);
                return 1;
            }
            draw_login(user, pass, field, clock, shell_hostname());
            continue;
        }
        if (k == '\b') {
            if (field == 0 && ulen > 0) user[--ulen] = 0;
            if (field == 1 && plen > 0) pass[--plen] = 0;
            draw_login(user, pass, field, clock, shell_hostname());
            continue;
        }
        if (k >= 32 && k < 127) {
            if (field == 0 && ulen < 31) { user[ulen++] = (char)k; user[ulen] = 0; }
            if (field == 1 && plen < 31) { pass[plen++] = (char)k; pass[plen] = 0; }
            draw_login(user, pass, field, clock, shell_hostname());
            continue;
        }
    }
}
