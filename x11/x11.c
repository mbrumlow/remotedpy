
#include "x11.h"

#include <stdlib.h>
#include <string.h>

static int gfr = 1;
static int nc = 0;

static int xerror_handler(Display *dpy, XErrorEvent *e) {
    return 0;
}

void DestroyImage(XXEvent *xe) {

    /*
    if(xe->image != NULL) {
	    XDestroyImage(xe->image);
        xe->image = NULL;
    }

    if(xe->data != NULL) {
        free(xe->data);
        xe->data = NULL;
    }
    */
}

void GetScreenSize(Display *dpy, int *width, int *height)  {
	int screen = DefaultScreen(dpy);
    *width = DisplayWidth(dpy, screen);
    *height = DisplayHeight(dpy, screen);
}


static void register_damage(Display *dpy, Window win) {
    XWindowAttributes attrib;
    if (XGetWindowAttributes(dpy, win, &attrib) && !attrib.override_redirect) {
        XDamageCreate(dpy, win, XDamageReportRawRectangles);
    }
}

void RegisterDamanges(Display *dpy) {

	Window root = DefaultRootWindow(dpy);
	XSelectInput(dpy, root, SubstructureNotifyMask);

    Window rootp, parent;
    Window *children;
    unsigned int i, nchildren;
    XQueryTree(dpy, root, &rootp, &parent, &children, &nchildren);

    XSetErrorHandler(xerror_handler);

	register_damage(dpy, root);
	for (i = 0; i < nchildren; i++) {
        register_damage(dpy, children[i]);
	}

    XFree(children);
}

void SendKey(Display *dpy, unsigned int key, int down) {

	Bool d = False;
	unsigned int kc = XKeysymToKeycode(dpy, key);

	if(down == 1) {
		d = True;
	}
	XTestFakeKeyEvent(dpy, kc, d, 0);
	XFlush(dpy);
}

void SendMouseClick(Display *dpy, int button, unsigned x, unsigned y, int down) {
	Bool d = False;
	if( down == 1) {
		d = True;
	}
	XTestFakeMotionEvent(dpy, 0, x, y, 0);
	XTestFakeButtonEvent(dpy, button, d, 0);
	XFlush(dpy);
}

void SendMouseMove(Display *dpy, unsigned x, unsigned y) {
	XTestFakeMotionEvent(dpy, 0, x, y, 0);
	XFlush(dpy);
}

int GetDamage(Display *dpy, int damageEvent, XXEvent *xxev)  {

    XEvent ev;
    Window root = DefaultRootWindow(dpy);
    int screen = DefaultScreen(dpy);

	while(1) {

        XNextEvent(dpy, &ev);

		if(ev.type == MapNotify){
            do { // Gobble up all these event types.
                register_damage(dpy, ev.xcreatewindow.window);
            } while ( XCheckTypedEvent(dpy, MapNotify, &ev) ) ;
			 continue;
		}

        XAnyEvent *any = (XAnyEvent *) &ev;

        int count = 0;
		if( ev.type == damageEvent + XDamageNotify ) {

            int screen = DefaultScreen(dpy);
            int width = DisplayWidth(dpy, screen);
            int height = DisplayHeight(dpy, screen);

			do { // Gobble up the rest of the damage events.
				XDamageNotifyEvent  *dev = (XDamageNotifyEvent *) &ev;
                XDamageSubtract( dpy, dev->damage, None, None );
            } while (XCheckTypedEvent(dpy, damageEvent + XDamageNotify, &ev));

            XImage *i = XGetImage(dpy, DefaultRootWindow(dpy), 0, 0, width, height, AllPlanes, ZPixmap);

            if( xxev->out == NULL ) {
                xxev->out = (unsigned int *) malloc(sizeof(unsigned int) * ((width * height) + 3));
            }

            memset(xxev->out, 0x0f, sizeof(unsigned int) * ((width * height) + 3));

            if( xxev->data == NULL ) {
                xxev->data = (unsigned int *) malloc(sizeof(unsigned int) * (width * height));
                memset(xxev->data, 0x0F, sizeof(unsigned int) * (width * height));
            }

            unsigned int *ndata = (unsigned int *) i->data;
            unsigned int *ldata = xxev->data;
            unsigned int *out = xxev->out;
            unsigned int *lh = out;

            out += 3;

            int z = 0; // positionn in the array.
            int s = 0; // stream start pixel.
            int c = 0; // count in stream.

            for(z = 1; z <= width * height; z++){

                if( (*ndata != *ldata) && (z < (width * height))  ) {

                    unsigned int n = *ndata & 0x00FFFFFF;

/*
                        switch(nc) {
                            case 0: n = n | 0x000000FF; break;
                            case 1: n = n | 0x0000FF00; break;
                            case 2: n = n | 0x00FF0000; break;
                        }
*/

                      *out++ = n;
                      c++;

                } else if(c > 0) {

                    *lh++ = (c + 2) | 2 << 24;
                    *lh++ = s;
                    *lh++ = (z - 1) - s;

                    s = z;
                    lh = out;

                    if( z < (width * height)) {
                        out += 3;
                    }

                    c=0;

                } else {
                    s = z;
                }

                ndata++;
                ldata++;

            }

            memcpy(xxev->data, i->data, sizeof(unsigned int) * (width * height));
            XDestroyImage(i);

            if(nc++ > 1) nc = 0;

            xxev->e = 1;
            xxev->x = 0;
            xxev->y = 0;
            xxev->w = width;
            xxev->h = height;
            xxev->l = (out - xxev->out);

            return 1;
        }
	}

    return 0;
}

