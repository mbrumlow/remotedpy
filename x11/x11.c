
#include "x11.h"

static int nc = 0;

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

struct pixel_buf *newpxb(int size) {
    struct pixel_buf *pxb;

    pxb = (PixelBuf *) malloc(sizeof(PixelBuf));
    if(!pxb)
        return NULL;

    pxb->buf = malloc(size);
    if(!pxb->buf)  {
        free(pxb);
        return NULL;
    }

    pxb->out = (uint32_t *) pxb->buf;
    pxb->pos = 0;
    pxb->size = size;
    pxb->pxsize = size / sizeof(uint32_t);
    pxb->dup = 0;
    pxb->ext = 0;

    return pxb;
}

inline rsetpxb(struct pixel_buf *pxb) {
    pxb->pos = 0;
    pxb->dup = 0;
}

inline int growpxb(struct pixel_buf  *pxb) {
    // TODO: realloc!
    printf("FUCK REALLOC: %d\n", pxb->pxsize);
}

inline int writepxb(struct pixel_buf *pxb, uint32_t p){
    int ret = 0;
    if(pxb->pos + 1 > pxb->pxsize) {
        ret = growpxb(pxb);
    }
    pxb->out[pxb->pos++] = p;
    pxb->dup = 0;
    return ret;
}

inline int writeduppxb(struct pixel_buf *pxb, uint32_t p){
    int ret = 0;
    if(pxb->pos + 1 > pxb->pxsize) {
        ret = growpxb(pxb);
    }

    if(pxb->ext == 1) {
        if( ((pxb->out[pxb->pos - 1] & 0x00FFFFFF) == p) ) {
            pxb->out[pxb->pos -2] = 0x80000000 | (3<<24) | ++pxb->dup;
        } else {
            pxb->out[pxb->pos++] = p | (1 << 24);
            pxb->dup = 1;
            pxb->ext = 0;
        }
    } else {
        if( pxb->dup > 0 && ((pxb->out[pxb->pos - 1] & 0x00FFFFFF) == p)){
            if( pxb->dup < 127 ) {
                pxb->out[pxb->pos - 1] = p | (++pxb->dup << 24);
            } else {
                pxb->out[pxb->pos -1] = 0x80000000 | (3<<24) | ++pxb->dup;
                pxb->out[pxb->pos++] = p | ( 0 << 24);
                pxb->ext = 1;
            }

        } else {
            pxb->out[pxb->pos++] = p | (1 << 24);
            pxb->dup = 1;
            pxb->ext = 0;
        }
    }

/*
    if( pxb->dup > 0 && ((pxb->out[pxb->pos - 1] & 0x00FFFFFF) == p) && (pxb->dup < 127)){
        pxb->out[pxb->pos - 1] = p | (++pxb->dup << 24);
    } else {
        pxb->out[pxb->pos++] = p | (1 << 24);
        pxb->dup = 1;
    }
 */

//    pxb->out[pxb->pos++] = p | (1 << 24);
    return ret;
}

XStream *NewXStream(Display *dpy) {

    XStream *xs;
    PixelBuf *pxb;
    int screen = DefaultScreen(dpy);
    int width = DisplayWidth(dpy, screen);
    int height = DisplayHeight(dpy, screen);
    int pxcount = width * height;

    pxcount = pxcount + (pxcount / 2);

    xs = (XStream *) malloc(sizeof(XStream));
    if(!xs)
        return NULL;

    pxb = newpxb(pxcount * sizeof(uint32_t));
    if(!pxb)
        return NULL;

    xs->pxb = pxb;
    xs->last = NULL;

    return xs;
}

void FreeXStream(XStream *xs) {

    if(!xs)
        return;

    if(xs->pxb) {
        free(xs->pxb);
        xs->pxb = NULL;
    }

    if(xs->last) {
        XDestroyImage(xs->last);
        xs->last = NULL;
    }

}

int GetDamage(Display *dpy, int damageEvent, XStream *xs)  {

    int i;
    int screen = DefaultScreen(dpy);
    int width = DisplayWidth(dpy, screen);
    int height = DisplayHeight(dpy, screen);
    int pxcount = width * height;

    XEvent ev;

    rsetpxb(xs->pxb);

	while(1) {

        XNextEvent(dpy, &ev);

		if(ev.type == MapNotify){
            do { // Gobble up all these event types.
                register_damage(dpy, ev.xcreatewindow.window);
            } while ( XCheckTypedEvent(dpy, MapNotify, &ev) ) ;
			 continue;
		}

		if( ev.type == damageEvent + XDamageNotify ) {

			do { // Gobble up the rest of the damage events.
                XDamageNotifyEvent  *dev = (XDamageNotifyEvent *) &ev;
				XDamageSubtract( dpy, dev->damage, None, None );
            } while (XCheckTypedEvent(dpy, damageEvent + XDamageNotify, &ev));

            XImage *img = XGetImage(dpy, DefaultRootWindow(dpy), 0, 0, width, height, AllPlanes, ZPixmap);

            // screen update - control | code | width | height
            writepxb(xs->pxb, 0x80000000 | (1<<24) |  (width<<12) | height);

            if(xs->last) {

                unsigned int *ldata = (uint32_t *) xs->last->data;;
                unsigned int *ndata = (uint32_t *) img->data;

                int needs_header = 1;
                for(i = 0; i <= pxcount; i++){

                    if(*ndata != *ldata) {

                        if(needs_header) {
                            // pixel offset - control | code | offset
                            writepxb(xs->pxb, 0x80000000 | (2<<24) |  i);
                            needs_header = 0;
                        }
                        writeduppxb(xs->pxb, *ndata & 0x00FFFFFF);

                    } else {

                        needs_header = 1;

                    }

                    ndata++;
                    ldata++;
                }

                XDestroyImage(xs->last);

            } else {

                unsigned int *data = (unsigned int *) img->data;
                for(i = 0; i < pxcount; i++){
                    writeduppxb(xs->pxb, *data++ & 0x00FFFFFF);
                }

            }

            writepxb(xs->pxb, 0x80000000 | (127<<24));
            xs->last = img;

            return 1;
        }
	}

    return 0;
}

