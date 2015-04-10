
#include "x11.h"

XImage *last = NULL;

void DestroyImage(XImage *img) {
	XDestroyImage(img);
}

void GetScreenSize(Display *dpy, int *width, int *height)  {
	int screen = DefaultScreen(dpy);
    *width = DisplayWidth(dpy, screen);
    *height = DisplayHeight(dpy, screen);
}

unsigned long GetPixel(XImage *img, int x, int y) {
	return XGetPixel(img, x, y);
}

static int xerror_handler(Display *dpy, XErrorEvent *e) {
    return 0;
}

static void register_damage(Display *dpy, Window win) {
    XWindowAttributes attrib;
  //  if (XGetWindowAttributes(dpy, win, &attrib) && !attrib.override_redirect) {
        XDamageCreate(dpy, win, XDamageReportRawRectangles);
//	}
}

void RegisterDamanges(Display *dpy) {

	Window root = DefaultRootWindow(dpy);
	XSelectInput(dpy, root, SubstructureNotifyMask);

    Window rootp, parent;
    Window *children;
    unsigned int i, nchildren;
    XQueryTree(dpy, root, &rootp, &parent, &children, &nchildren);

    XSetErrorHandler(xerror_handler);

 	XCompositeRedirectSubwindows( dpy, RootWindow( dpy, 0 ), CompositeRedirectAutomatic );

	//register_damage(dpy, root);
	register_damage(dpy, 4210123);

    XSelectInput(dpy,4210123 ,StructureNotifyMask);

//	printf("REGISTER: %ld\n", root);
	for (i = 0; i < nchildren; i++) {
//		printf("REGISTER: %ld\n", children[i]);
//		register_damage(dpy, children[i]);
		//break;
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

int NextEvent(Display *dpy, int damageEvent, XXEvent *xxev) {
    XEvent ev;

    while(1) {
        XNextEvent(dpy, &ev);

        if ( ev.type == ConfigureNotify ) {
            XConfigureEvent *xce = (XConfigureEvent *) &ev;

            do {
                break;
            } while ( XCheckTypedWindowEvent(dpy, xce->window,  ConfigureNotify, &ev) );

            xxev->e = 2;
            xxev->window = xce->window;
            xxev->x = xce->x;
            xxev->y = xce->y;
            xxev->h = xce->height + xce->border_width;
            xxev->w = xce->width + xce->border_width;

            return XPending(dpy);
        }

        // TODO: Handle in its own function.
        if( ev.type == damageEvent + XDamageNotify ) {

            XAnyEvent *any = (XAnyEvent *) &ev;

            int x, y, w, h;
            int rx = 0;
			int ry = 0;
			int fr = 0;
			int damage = 0;

            do {

                XDamageNotifyEvent  *dev = (XDamageNotifyEvent *) &ev;
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
            } while ( XCheckTypedWindowEvent(dpy, any->window,  damageEvent + XDamageNotify, &ev) );

            h = ry - y;
            w = rx - x;

            XImage *i = XGetImage(dpy, any->window, x, y, w, h, AllPlanes, ZPixmap);

            if(any->window == 4210123) {
                if(last == NULL) {
                    last =  XGetImage(dpy, any->window, x, y, w, h, AllPlanes, ZPixmap);
                } else {

                    if(i->width == last->width && i->height == last->height) {
                    // int memcmp(const void *s1, const void *s2, size_t n);
                        if( memcmp(i->data, last->data, ((i->width * i->height) * 32 )/  8) == 0) {
                           printf("THE SAME SKIPPING\n");
                            continue;
                        } else {
                            printf("NOT THE SAME\n");
                           XDestroyImage(last);
                           last = XGetImage(dpy, any->window, x, y, w, h, AllPlanes, ZPixmap);
                        }

                   }
                }
            }

            xxev->e = 1;
            xxev->window = any->window;
            xxev->image = i;
            xxev->x = x;
            xxev->y = y;
            xxev->w = w;
            xxev->h = h;

            return XPending(dpy);
        }
    }
}

XImage *GetDamage(Display *dpy, int damageEvent, int *x, int *y, int *h, int *w)  {

    XEvent ev;

    int screen = DefaultScreen(dpy);

	while(1) {

		XNextEvent(dpy, &ev);

		if(ev.type == MapNotify){
			// register_damage(dpy, ev.xcreatewindow.window);
			 continue;
		}

		if( ev.type == damageEvent + XDamageNotify ) {
			XAnyEvent *any = (XAnyEvent *) &ev;

			int rx = 0;
			int ry = 0;
			int fr = 0;
			int damage = 0;

			do {
				XDamageNotifyEvent  *dev = (XDamageNotifyEvent *) &ev;
				// TODO: split large non-overlapping regions up instead of
				// of making one large region.

				if(fr == 0) {
					*x = dev->area.x;
					*w = dev->area.width;
					rx = dev->area.x + dev->area.width;
				} else if(dev->area.x < *x) {
        			*x = dev->area.x;
				}

				if(fr == 0) {
					*y = dev->area.y;
					*h = dev->area.height;
					ry = dev->area.y + dev->area.height;
				} else if(dev->area.y < *y) {
        			*y = dev->area.y;
				}

				if( dev->area.y + dev->area.height > ry ) {
					ry = dev->area.y + dev->area.height;
					*h = ry - *y;
				}

				if( dev->area.x + dev->area.width > rx ) {
					rx = dev->area.x  + dev->area.width;
					*w = rx - *x;
				}

				if(*x < 0) {
					*x = 0;
				}

				if(*y < 0) {
					*y = 0;
				}

				fr = 1;
        		damage = 1;
				XDamageSubtract( dpy, dev->damage, None, None );
				break;
			} while (XCheckTypedEvent(dpy, damageEvent + XDamageNotify, &ev));

			if(damage) {

				*h = ry - *y;
				*w = rx - *x;


				XImage *i = XGetImage(dpy,
					any->window,
                	*x, *y, *w, *h,
                	AllPlanes, ZPixmap);

				XWindowAttributes xwa;
				XGetWindowAttributes(dpy, any->window, &xwa);

				//*x = xwa.x;
				//*y = xwa.y;

				return i;
			}
    	}
	}

	return NULL;
}

