package x11

/*
#include "x11.h"
#cgo LDFLAGS: -lX11 -lXdamage -lXcomposite -lXtst
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

func (d *Display) SendMouseMove(x, y uint32) {
	C.SendMouseMove(d.dpy2, C.uint(x), C.uint(y))
}

func (d *Display) SendMouseClick(button int, x, y uint32, down bool) {
	downInt := 0
	if down {
		downInt = 1
	}
	C.SendMouseClick(d.dpy2, C.int(button), C.uint(x), C.uint(y), C.int(downInt))
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
