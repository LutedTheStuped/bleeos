/* Tiny compositor: direct-to-LFB redraw on dirty flag + 500ms tick.
 * Layout: border 2px, titlebar 20px, close 16x16 top-right. */
#include "wm.h"
#include "vbe.h"
#include "gfx.h"
#include "mouse.h"

#define MAXWIN 8
#define TITLE_H 20
#define BORDER 2

#define C_DESK   RGB(16, 32, 64)
#define C_DESK2  RGB(20, 40, 80)
#define C_BAR    RGB(8, 16, 32)
#define C_BORD   RGB(180, 190, 200)
#define C_TACT   RGB(30, 90, 200)
#define C_TINACT RGB(110, 120, 130)
#define C_TTEXT  RGB(255, 255, 255)
#define C_CLOSE  RGB(200, 60, 50)
#define C_CLIENT RGB(240, 240, 235)
#define C_CURSOR RGB(255, 255, 255)
#define C_COUT   RGB(0, 0, 0)

static win_t wins[MAXWIN];
static int order[MAXWIN];   /* bottom->top window indices */
static int norder;
static int mx, my, mbtn;
static int dragging;
static int drag_dx, drag_dy;
static int dirty = 1;
static int quit;

void wm_dirty(void) { dirty = 1; }
void wm_mouse_xy(int *x, int *y) { *x = mx; *y = my; }
int wm_nwin(void) { return norder; }

win_t *wm_open(const char *title, int x, int y, int w, int h,
               void (*draw)(win_t *, int, int),
               void (*click)(win_t *, int, int, int), void *data) {
    for (int i = 0; i < MAXWIN; i++) {
        if (wins[i].used) continue;
        win_t *w_ = &wins[i];
        w_->used = 1; w_->x = x; w_->y = y; w_->w = w; w_->h = h;
        w_->id = i; w_->data = data; w_->draw = draw; w_->click = click;
        int k = 0;
        while (title[k] && k < 31) { w_->title[k] = title[k]; k++; }
        w_->title[k] = 0;
        if (norder < MAXWIN) order[norder++] = i;   /* new window on top */
        dirty = 1;
        return w_;
    }
    return 0;
}

static void wm_close(int idx) {
    wins[idx].used = 0;
    for (int i = 0; i < norder; i++) {
        if (order[i] == idx) {
            for (int j = i; j + 1 < norder; j++) order[j] = order[j + 1];
            norder--;
            break;
        }
    }
    if (dragging) dragging = 0;
    dirty = 1;
    if (norder == 0) quit = 1;
}

static void wm_focus(int idx) {
    for (int i = 0; i < norder; i++) {
        if (order[i] == idx) {
            for (int j = i; j + 1 < norder; j++) order[j] = order[j + 1];
            order[norder - 1] = idx;
            dirty = 1;
            return;
        }
    }
}

/* topmost window containing point, or -1 */
static int win_at(int x, int y) {
    for (int i = norder - 1; i >= 0; i--) {
        win_t *w = &wins[order[i]];
        if (x >= w->x && y >= w->y && x < w->x + w->w && y < w->y + w->h)
            return order[i];
    }
    return -1;
}

static int in_close(win_t *w, int x, int y) {
    return x >= w->x + w->w - 18 && x < w->x + w->w - 2 &&
           y >= w->y + 2 && y < w->y + 18;
}
static int in_title(win_t *w, int x, int y) {
    return x >= w->x && y >= w->y && x < w->x + w->w && y < w->y + TITLE_H;
}

static void draw_cursor(void) {
    /* arrow: white with black outline */
    static const char *rows[10] = {
        "X.........", "XX........", "XXX.......", "XXXX......",
        "XXXXX.....", "XXXXXX....", "XXXXXXX...", "XXXXXXXX..",
        "XXXXXXXXX.", "XXXX......",
    };
    for (int r = 0; r < 10; r++) {
        for (int c = 0; rows[r][c]; c++) {
            if (rows[r][c] != 'X') continue;
            gfx_pixel(mx + c, my + r, C_COUT);
            gfx_pixel(mx + c + 1, my + r + 1, C_CURSOR);
        }
    }
    gfx_pixel(mx, my, C_CURSOR);
}

static void draw_all(void) {
    int W = gfx_w(), H = gfx_h();
    gfx_noclip();
    /* desktop: two-tone bands */
    for (int y = 0; y < H; y++)
        gfx_hline(0, y, W, (y & 16) ? C_DESK : C_DESK2);
    /* hint bar */
    gfx_fill(0, H - 22, W, 22, C_BAR);
    gfx_hline(0, H - 22, W, C_BORD);
    gfx_text(8, H - 15, "BleeOS WM  Esc: exit shell   Click: focus/drag   X: close",
             RGB(200, 210, 220), GFX_TRANS);
    for (int oi = 0; oi < norder; oi++) {
        win_t *w = &wins[order[oi]];
        int top = (oi == norder - 1);
        u32 bar = top ? C_TACT : C_TINACT;
        gfx_fill(w->x, w->y, w->w, w->h, C_BORD);              /* border */
        gfx_fill(w->x + BORDER, w->y + BORDER,
                 w->w - 2 * BORDER, TITLE_H - BORDER, bar);    /* title */
        gfx_text(w->x + 8, w->y + 2 + 6, w->title, C_TTEXT, GFX_TRANS);
        /* close button */
        int bx = w->x + w->w - 18, by = w->y + 2;
        gfx_fill(bx, by, 16, 16, C_CLOSE);
        gfx_text(bx + 4, by + 4, "X", C_TTEXT, GFX_TRANS);
        /* client */
        int cx = w->x + BORDER, cy = w->y + TITLE_H;
        gfx_fill(cx, cy, w->w - 2 * BORDER, w->h - TITLE_H - BORDER, C_CLIENT);
        if (w->draw) {
            gfx_clip(cx, cy, w->w - 2 * BORDER, w->h - TITLE_H - BORDER);
            w->draw(w, cx, cy);
            gfx_noclip();
        }
    }
    draw_cursor();
}

static void on_button(int down) {
    if (down) {
        int idx = win_at(mx, my);
        if (idx < 0) { dragging = 0; return; }
        wm_focus(idx);
        win_t *w = &wins[idx];
        if (in_close(w, mx, my)) { wm_close(idx); return; }
        if (in_title(w, mx, my)) {
            dragging = 1;
            drag_dx = mx - w->x; drag_dy = my - w->y;
            return;
        }
        /* client click */
        if (w->click)
            w->click(w, mx - (w->x + BORDER), my - (w->y + TITLE_H), mbtn);
        dirty = 1;
    } else {
        dragging = 0;
    }
}

int wm_init(void) {
    if (vbe_set(640, 480, 32)) return 1;
    gfx_init((u32 *)vbe_lfb(), vbe_width(), vbe_height());
    for (int i = 0; i < MAXWIN; i++) wins[i].used = 0;
    norder = 0; dragging = 0; quit = 0; dirty = 1;
    mx = gfx_w() / 2; my = gfx_h() / 2; mbtn = 0;
    if (mouse_init()) { vbe_disable(); return 2; }
    return 0;
}

void wm_run(void) {
    extern void apps_open_demo(void);
    int last_btn = 0;
    u32 tick = 0;
    apps_open_demo();
    draw_all();
    for (;;) {
        int dx, dy, btn;
        while (mouse_poll(&dx, &dy, &btn)) {
            mx += dx; my -= dy;
            if (mx < 0) mx = 0;
            if (my < 0) my = 0;
            if (mx >= gfx_w()) mx = gfx_w() - 1;
            if (my >= gfx_h()) my = gfx_h() - 1;
            mbtn = btn;
            if ((btn & 1) && !(last_btn & 1)) on_button(1);
            else if (!(btn & 1) && (last_btn & 1)) on_button(0);
            else if (dragging) {
                int idx = norder ? order[norder - 1] : -1;
                if (idx >= 0) {
                    win_t *w = &wins[idx];
                    w->x = mx - drag_dx; w->y = my - drag_dy;
                    if (w->x < 0) w->x = 0;
                    if (w->y < 0) w->y = 0;
                    if (w->x + w->w > gfx_w()) w->x = gfx_w() - w->w;
                    if (w->y + w->h > gfx_h()) w->y = gfx_h() - w->h;
                }
            }
            last_btn = btn;
            dirty = 1;
        }
        int k = kbd_trykey();
        if (k == 27) break;             /* Esc exits */
        if (quit) break;
        tick++;
        if (dirty || (tick & 31) == 0) { draw_all(); dirty = 0; }
        sleep_ms(10);
    }
    vbe_disable();
}
