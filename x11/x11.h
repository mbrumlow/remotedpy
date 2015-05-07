
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>

typedef struct pixel_buf {
    char     *buf;
    uint32_t *out;
    int      size;
    int      pxsize;
    int      pos;
    int      dup;
} PixelBuf;

typedef struct XStream {
    PixelBuf *pxb;
    XImage *last;

} XStream;

typedef struct XXEvent {
    int e;
    int x;
    int y;
    int w;
    int h;
    int l;
    XImage *image;
} XXEvent;



void DestroyImage(XImage *img);
void GetScreenSize(Display *dpy, int *width, int *height);
static void register_damage(Display *dpy, Window win);
void RegisterDamanges(Display *dpy);
void SendKey(Display *dpy, unsigned int key, int down);
void SendMouseClick(Display *dpy, int button, unsigned x, unsigned y, int down);
void SendMouseMove(Display *dpy, unsigned x, unsigned y);

XStream *NewXStream(Display *dpy);
void FreeXStream(XStream *xs);
int GetDamage(Display *dpy, int damageEvent, XStream *xs);


