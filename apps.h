/* Demo apps for the window manager. */
#ifndef APPS_H
#define APPS_H

void apps_open_demo(void);   /* opens Counter + SysInfo */
void apps_open_calc(void);
void apps_open_display(void);
int  apps_custom_key(int k);  /* custom-res editor: 1 = key consumed */

#endif
