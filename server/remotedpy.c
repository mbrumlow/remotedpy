
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/tcp.h>

#include <X11/Xlib.h>
#include <X11/extensions/Xdamage.h>
#include <X11/extensions/Xcomposite.h>
#include <X11/keysym.h>
#include <X11/extensions/XTest.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

void runclient(int fd);

struct pixel_buf {
    char     *buf;
    uint32_t *out;
    int      size;
    int      pxsize;
    int      pos;
    int      dup;
};

struct timespec timer_start(){
    struct timespec start_time;
    clock_gettime(CLOCK_MONOTONIC, &start_time);
    return start_time;
}

long timer_end(struct timespec start_time){
    struct timespec end_time;
    clock_gettime(CLOCK_MONOTONIC, &end_time);
    long diffInNanos = end_time.tv_nsec - start_time.tv_nsec + (1000000000 * (end_time.tv_sec - start_time.tv_sec));
    return diffInNanos;
}



static int xerror_handler(Display *dpy, XErrorEvent *e) {
    return 0;
}

static void register_damage(Display *dpy, Window win) {
    XWindowAttributes attrib;
    if (XGetWindowAttributes(dpy, win, &attrib) && !attrib.override_redirect) {
        //XDamageCreate(dpy, win, XDamageReportRawRectangles);
        XDamageCreate(dpy, win, XDamageReportBoundingBox);
    }
}

void register_damanges(Display *dpy) {

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


int main(int argc, char *argv[]) {

    int sockfd, fd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;

     if (argc < 2) {
         fprintf(stderr,"ERROR, no port provided\n");
         exit(1);
     }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        perror("creating socket");
        exit(1);
    }

    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        perror("binding");
        exit(1);
    }

    listen(sockfd,5);
    clilen = sizeof(cli_addr);

    while (1) {
        fd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
        if (fd < 0) {
            perror("accept");
            continue;
        }

        pid = fork();
        if( pid < 0 ) {
            perror("fork");
            continue;
        }
        if( pid == 0 ){
            close(sockfd);
            runclient(fd);
            exit(0);
        } else {
            close(fd);
        }
    }

    return 0;
}

struct pixel_buf *newpxb(int size) {
    struct pixel_buf *pxb;

    pxb = (struct pixel_buf *) malloc(sizeof(struct pixel_buf));
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

    return pxb;
}

inline int flushpxb(int fd, struct pixel_buf *pxb) {
    if(pxb->pos == 0)
        return 0;

    int state = 1;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));
    int ret = write(fd, pxb->buf, pxb->pos * sizeof(uint32_t));
    state = 0;
    setsockopt(fd, IPPROTO_TCP, TCP_CORK, &state, sizeof(state));

    pxb->pos = 0;
    pxb->dup = 0;
    return ret;
}

inline int writepxb(int fd, struct pixel_buf *pxb, uint32_t p){
    int ret = 0;
    if(pxb->pos + 1 > pxb->pxsize) {
        ret = flushpxb(fd, pxb);
    }
    pxb->out[pxb->pos++] = p;
    pxb->dup = 0;
    return ret;
}

inline int writeduppxb(int fd, struct pixel_buf *pxb, uint32_t p){
    int ret = 0;
    if(pxb->pos + 1 > pxb->pxsize) {
        ret = flushpxb(fd, pxb);
    }

    if( pxb->dup > 0 && ((pxb->out[pxb->pos - 1] & 0x00FFFFFF) == p) && (pxb->dup < 127)){
        pxb->out[pxb->pos - 1] = p | (++pxb->dup << 24);
    } else {
        pxb->out[pxb->pos++] = p | (1 << 24);
        pxb->dup = 1;
    }
    return ret;
}

void runclient(int fd) {

    printf("client connected\n");

    int err;
    int damageEvent;
    Display *dpy;

    dpy = XOpenDisplay(":0");

    if(!XDamageQueryExtension(dpy, &damageEvent, &err)) {
        fprintf(stderr, "XDamage extention not found!");
    //    return -1;
    }

    register_damanges(dpy);

    int screen = DefaultScreen(dpy);
    int width = DisplayWidth(dpy, screen);
    int height = DisplayHeight(dpy, screen);
    int pxcount = width * height;
    XImage *last = NULL;

    struct pixel_buf *pxb = newpxb(32768);
    if(!pxb) {
        fprintf(stderr, "Failed to allocate pxb!\n");
        return;
    }

    int i;
    XEvent ev;
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

            struct timespec vartime = timer_start();

            XImage *img = XGetImage(dpy, DefaultRootWindow(dpy), 0, 0, width, height, AllPlanes, ZPixmap);

            // screen update - control | code | width | height
            writepxb(fd, pxb, 0x80000000 | (1<<24) |  (width<<12) | height );

            if(last) {

                unsigned int *ldata = (unsigned int *) last->data;;
                unsigned int *ndata = (unsigned int *) img->data;

                int needs_header = 1;
                for(i = 0; i <= pxcount; i++){
                    if(*ndata != *ldata) {
                        if(needs_header) {
                            // pixel offset - control | code | offset
                            writepxb(fd, pxb, 0x80000000 | (2<<24) |  i);
                            needs_header = 0;
                        }
                        writeduppxb(fd, pxb, *ndata & 0x00FFFFFF);
                    } else {
                        needs_header = 1;
                    }

                    ndata++;
                    ldata++;
                }

                XDestroyImage(last);

            } else {

                unsigned int *data = (unsigned int *) img->data;

                for(i = 0; i < pxcount; i++){
                    writeduppxb(fd, pxb, *data++ & 0x00FFFFFF);
                }
            }

            writepxb(fd, pxb, 0x80000000 | (127<<24));
            flushpxb(fd, pxb);

            last = img;

            long time_elapsed_nanos = timer_end(vartime);
            if(time_elapsed_nanos/ 1000000 > 33) {
                printf("screen update (ms): %ld\n", time_elapsed_nanos/ 1000000);
            }
        }
    }
}


