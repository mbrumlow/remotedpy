package x11

/*
#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#cgo LDFLAGS: -lX11 -lXdamage -lXcomposite -lXtst

Pixmap windowPix;

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

 	XCompositeRedirectSubwindows( dpy, RootWindow( dpy, i ), CompositeRedirectAutomatic );

	register_damage(dpy, root);
	for (i = 0; i < nchildren; i++) {
		register_damage(dpy, children[i]);
	}

    XFree(children);
}

void SendKey(Display *dpy, unsigned int key, int down) {

	unsigned int kc = XKeysymToKeycode(dpy, key);

	if(down == 0) {
		XTestFakeKeyEvent(dpy, kc, False, 0);
	} else{
		XTestFakeKeyEvent(dpy, kc, True, 0);
	}
	XFlush(dpy);
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
			} while (XCheckTypedEvent(dpy, damageEvent + XDamageNotify, &ev));

			if(damage) {

				*h = ry - *y;
				*w = rx - *x;

//				*x = 0;
//				*y = 0;
//				*w = 800;
//				*h = 600;

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
	"reflect"
	"time"
	"unsafe"
)

type Display struct {
	dpy         *C.struct__XDisplay
	dpy2        *C.struct__XDisplay
	damageEvent C.int
	C           chan Image
}

type Image struct {
	img *C.struct__XImage
	S   []C.uint
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

	dpy2 := C.XOpenDisplay(display)
	if dpy2 == nil {
		return nil, errors.New("Failed to open display.")
	}

	var err C.int
	var damageEvent C.int
	if C.XDamageQueryExtension(dpy, &damageEvent, &err) == C.int(0) {
		C.XCloseDisplay(dpy)
		return nil, errors.New("XDamage extention not found!")
	}

	C.RegisterDamanges(dpy)

	d := &Display{dpy: dpy, dpy2: dpy2, damageEvent: damageEvent}
	d.C = make(chan Image, 2)

	return d, nil
}

func (d *Display) GetScreenSize() (int, int) {

	var w C.int
	var h C.int

	C.GetScreenSize(d.dpy, &w, &h)

	return int(w), int(h)
}

func (d *Display) StartStream() {
	go d.getChanges()
}

func (d *Display) SendKey(key uint32, down bool) {
	downInt := 0
	if down {
		downInt = 1
	}
	C.SendKey(d.dpy2, C.uint(key), C.int(downInt))
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

			length := (int(w) * int(h))
			hdr := reflect.SliceHeader{
				Data: uintptr(unsafe.Pointer(img.data)),
				Len:  length,
				Cap:  length,
			}
			goSlice := *(*[]C.uint)(unsafe.Pointer(&hdr))

			i := Image{img: img, X: int(x), Y: int(y), H: int(h), W: int(w), S: goSlice}
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
