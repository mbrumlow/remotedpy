package x11

/*
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#cgo LDFLAGS: -lX11 -lXdamage

void DestroyImage(XImage *img) {
	XDestroyImage(img);
}

unsigned long GetPixel(XImage *img, int x, int y) {
	return XGetPixel(img, x, y);
}

static int xerror_handler(Display *dpy, XErrorEvent *e) {
    return 0;
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

XImage *GetDamage(Display *dpy, int damageEvent, int *x, int *y, int *h, int *w)  {

    XEvent ev;

    int screen = DefaultScreen(dpy);

	while(1) {

		XNextEvent(dpy, &ev);

		if(ev.type == MapNotify){
			 register_damage(dpy, ev.xcreatewindow.window);
			 continue;
		}

		if( ev.type == damageEvent + XDamageNotify ) {

			int rx = 0;
			int ry = 0;
			int fr = 0;
			int damage = 0;

			do {
				XDamageNotifyEvent  *dev = (XDamageNotifyEvent *) &ev;

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

				fr = 1;
        		damage = 1;
			} while (XCheckTypedEvent(dpy, damageEvent + XDamageNotify, &ev));

			if(damage) {

				*h = ry - *y;
				*w = rx - *x;

				return XGetImage(dpy,
					DefaultRootWindow(dpy),
                	*x, *y, *w, *h,
                	AllPlanes, ZPixmap);
			}

    	}

	}

	return NULL;
}

*/
import "C"

import (
	"errors"
	"time"
)

type Display struct {
	dpy         *C.struct__XDisplay
	damageEvent C.int
	C           chan Image
}

type Image struct {
	img *C.struct__XImage
	X   int
	Y   int
	W   int
	H   int
}

func OpenDisplay() (*Display, error) {

	display := C.CString(":0")
	dpy := C.XOpenDisplay(display)
	if dpy == nil {
		return nil, errors.New("Failed to open display.")
	}

	var err C.int
	var damageEvent C.int
	if C.XDamageQueryExtension(dpy, &damageEvent, &err) == C.int(0) {
		C.XCloseDisplay(dpy)
		return nil, errors.New("XDamage extention not found!")
	}

	C.RegisterDamanges(dpy)

	d := &Display{dpy: dpy, damageEvent: damageEvent}
	d.C = make(chan Image, 2)

	return d, nil
}

func (d *Display) StartStream() {
	go d.getChanges()
}

func (d *Display) StopStream() {
	// TODO: Send event to X and shutdown the getChanges.
}

func (d *Display) getChanges() {

	var x C.int
	var y C.int
	var h C.int
	var w C.int

	ticker := time.NewTicker(time.Millisecond * 10)
	defer ticker.Stop()

	for _ = range ticker.C {
		img := C.GetDamage(d.dpy, d.damageEvent, &x, &y, &h, &w)
		if img != nil {
			i := Image{img: img, X: int(x), Y: int(y), H: int(h), W: int(w)}
			d.C <- i
		}
	}
}

func (i *Image) GetPixel(x, y int) uint32 {
	cx := C.int(x)
	cy := C.int(y)
	cr := C.GetPixel(i.img, cx, cy)
	ret := uint32(cr)
	return ret
}

func (i *Image) DistoryImage() {
	C.DestroyImage(i.img)
}

func (d *Display) Close() {
	C.XCloseDisplay(d.dpy)
}
