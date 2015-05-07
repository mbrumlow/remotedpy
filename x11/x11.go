package x11

/*
#include "x11.h"
#cgo CFLAGS: -O3
#cgo LDFLAGS: -lX11 -lXdamage -lXcomposite -lXtst
*/
import "C"

import (
	"bytes"
	"encoding/binary"
	"errors"
	"fmt"
	"reflect"
	"unsafe"

	"github.com/pierrec/lz4"
)

type Display struct {
	dpy         *C.struct__XDisplay
	dpy2        *C.struct__XDisplay
	xs          *C.XStream
	damageEvent C.int
	S           chan *bytes.Buffer
}

func OpenDisplay() (*Display, error) {

	display := C.CString(":20")
	dpy := C.XOpenDisplay(display)
	if dpy == nil {
		return nil, errors.New("Failed to open display.")
	}

	dpy2 := C.XOpenDisplay(display)
	if dpy2 == nil {
		return nil, errors.New("Failed to open display.")
	}

	xs := C.NewXStream(dpy)
	if xs == nil {
		return nil, errors.New("Failed to get xstrea.")
	}

	var err C.int
	var damageEvent C.int
	if C.XDamageQueryExtension(dpy, &damageEvent, &err) == C.int(0) {
		C.XCloseDisplay(dpy)
		return nil, errors.New("XDamage extention not found!")
	}

	C.RegisterDamanges(dpy)

	d := &Display{dpy: dpy, dpy2: dpy2, damageEvent: damageEvent, xs: xs}
	d.S = make(chan *bytes.Buffer, 4)

	return d, nil
}

func (d *Display) GetScreenSize() (int, int) {

	var w C.int
	var h C.int

	C.GetScreenSize(d.dpy, &w, &h)

	return int(w), int(h)
}

func (d *Display) StartStream() {
	go d.getStreamBuffer()
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

func (d *Display) getStreamBuffer() {

	/*
		ticker := time.NewTicker(time.Millisecond * 2)
		defer ticker.Stop()

		for _ = range ticker.C {
	*/

	for {
		ret := C.GetDamage(d.dpy, d.damageEvent, d.xs)
		if ret == C.int(0) {
			continue
		}

		hdr := reflect.SliceHeader{
			Data: uintptr(unsafe.Pointer(d.xs.pxb.out)),
			Len:  int(d.xs.pxb.pos),
			Cap:  int(d.xs.pxb.pos),
		}
		goSlice := *(*[]C.uint)(unsafe.Pointer(&hdr))

		bb := new(bytes.Buffer)
		binary.Write(bb, binary.LittleEndian, goSlice)

		bb2 := new(bytes.Buffer)
		zw := lz4.NewWriter(bb2)
		binary.Write(zw, binary.LittleEndian, goSlice)

		//		io.Copy(zw, bb)

		fmt.Printf("SIZE: %v\n", len(bb.Bytes())-len(bb2.Bytes()))

		d.S <- bb2
	}
}

func (d *Display) Close() {
	C.XCloseDisplay(d.dpy)
	C.FreeXStream(d.xs)
}
