
#include <stdio.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

typedef struct XXEvent {
    int e;
    int x;
    int y;
    int w;
    int h;
    XImage *image;
} XXEvent;

void DestroyImage(XImage *img);
void GetScreenSize(Display *dpy, int *width, int *height);
static void register_damage(Display *dpy, Window win);
void RegisterDamanges(Display *dpy);
void SendKey(Display *dpy, unsigned int key, int down);
void SendMouseClick(Display *dpy, int button, unsigned x, unsigned y, int down);
void SendMouseMove(Display *dpy, unsigned x, unsigned y);
int GetDamage(Display *dpy, int damageEvent, XXEvent *xxev);


