package x11

/*
#include "x11.h"
#cgo LDFLAGS: -lX11 -lXdamage -lXcomposite -lXtst
*/
import "C"

import (
	"bytes"
	"encoding/binary"
	"errors"
)

type Display struct {
	dpy         *C.struct__XDisplay
	dpy2        *C.struct__XDisplay
	damageEvent C.int
	S           chan *bytes.Buffer
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
	d.S = make(chan *bytes.Buffer)

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

func (d *Display) getInitalStreamBuffer() *bytes.Buffer {

	bb := new(bytes.Buffer)

	sw, sh := d.GetScreenSize()
	binary.Write(bb, binary.LittleEndian, uint32(1|(1<<24)))
	binary.Write(bb, binary.LittleEndian, uint32(sw|sh<<16))

	return bb
}

func (d *Display) getStreamBuffer() {

	d.S <- d.getInitalStreamBuffer()

	for {
		var xe C.XXEvent
		ret := C.GetDamage(d.dpy, d.damageEvent, &xe)
		if ret == C.int(0) {
			continue
		}

		switch int(xe.e) {
		case 1:
			d.S <- DamageStream(&xe)
		}
	}
}

func (d *Display) Close() {
	C.XCloseDisplay(d.dpy)
}
