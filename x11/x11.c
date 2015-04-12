
#include "x11.h"

static int xerror_handler(Display *dpy, XErrorEvent *e) {
    return 0;
}

void DestroyImage(XImage *img) {
	XDestroyImage(img);
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

    int screen = DefaultScreen(dpy);

	while(1) {

        XNextEvent(dpy, &ev);

		if(ev.type == MapNotify){
            do { // Gobble up all these event types.
                register_damage(dpy, ev.xcreatewindow.window);
            } while ( XCheckTypedEvent(dpy, MapNotify, &ev) ) ;
			 continue;
		}

		if( ev.type == damageEvent + XDamageNotify ) {

			int rx = 0;
			int ry = 0;
			int fr = 0;
            int x, y, w, h;

			do { // Gobble up the rest of the damage events.

                XDamageNotifyEvent  *dev = (XDamageNotifyEvent *) &ev;

            	// TODO: split large non-overlapping regions up instead of
				// of making one large region.

				if(fr == 0) {
					x = dev->area.x;
					w = dev->area.width;
					rx = dev->area.x + dev->area.width;
				} else if(dev->area.x < x) {
        			x = dev->area.x;
				}

				if(fr == 0) {
					y = dev->area.y;
					h = dev->area.height;
					ry = dev->area.y + dev->area.height;
				} else if(dev->area.y < y) {
        			y = dev->area.y;
				}

				if( dev->area.y + dev->area.height > ry ) {
					ry = dev->area.y + dev->area.height;
					h = ry - y;
				}

				if( dev->area.x + dev->area.width > rx ) {
					rx = dev->area.x  + dev->area.width;
					w = rx - x;
				}

                // TODO: Figure out what -1 really means.
				if(x < 0) x = 0;
				if(y < 0) y = 0;

				fr = 1;
				XDamageSubtract( dpy, dev->damage, None, None );
            } while (XCheckTypedEvent(dpy, damageEvent + XDamageNotify, &ev));

            h = ry - y;
            w = rx - x;

            XImage *i = XGetImage(dpy, DefaultRootWindow(dpy), x, y, w, h, AllPlanes, ZPixmap);

            // Going to make some attemt to reduce the number of pixels we have to send
            // This will compress any connsecitive pixels into a single pixel and use
            // the alpha channel to store the number of pixels following that have
            // the same color.
            //
            unsigned int *pix = (unsigned int *) i->data;
            int z = 0; // positionn in the array.
            int c = 0; // same color count.
            int l = 0; // last position in the compressed version.
            int p = pix[0] & 0x00FFFFF; // First pixle with alpha channel cleared.

            for(z = 1; z <= w * h; z ++ ) {
                unsigned int n = pix[z] & 0x00FFFFFF;
                if( n == p && c < 255 && z < w * h ) {
                    c++;
                } else {
                    pix[l] = (c << 24) | (p & 0x00FFFFFF);
                    p = n;
                    c = 0;
                    l++;
                }
            }

            xxev->e = 1;
            xxev->x = x;
            xxev->y = y;
            xxev->w = w;
            xxev->h = h;
            xxev->l = l;
            xxev->image = i;

            return 1;
        }
	}

    return 0;
}

