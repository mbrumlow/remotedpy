#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>


void DestroyImage(XImage *img);
void GetScreenSize(Display *dpy, int *width, int *height);
unsigned long GetPixel(XImage *img, int x, int y);
static int xerror_handler(Display *dpy, XErrorEvent *e);
static void register_damage(Display *dpy, Window win);
void RegisterDamanges(Display *dpy);
void SendKey(Display *dpy, unsigned int key, int down);
void SendMouseClick(Display *dpy, int button, unsigned x, unsigned y, int down);
void SendMouseMove(Display *dpy, unsigned x, unsigned y);
XImage *GetDamage(Display *dpy, int damageEvent, int *x, int *y, int *h, int *w);

