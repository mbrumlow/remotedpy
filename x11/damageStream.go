package x11

/*
#include "x11.h"
#cgo LDFLAGS: -lX11 -lXdamage -lXcomposite -lXtst
*/
import "C"

import (
	"bytes"
	"encoding/binary"
	"reflect"
	"unsafe"
)

func DamageStream(xe *C.XXEvent) *bytes.Buffer {

	defer C.DestroyImage(xe)

	hdr := reflect.SliceHeader{
		Data: uintptr(unsafe.Pointer(xe.out)),
		Len:  int(xe.l),
		Cap:  int(xe.l),
	}
	goSlice := *(*[]C.uint)(unsafe.Pointer(&hdr))

	//	fmt.Printf("SENDING: %v insteaad of %v, savings: %v\n", int(xe.l), int(xe.w*xe.h), int(xe.w*xe.h)-int(xe.l))

	bb := new(bytes.Buffer)
	//	binary.Write(bb, binary.LittleEndian, uint32(len(goSlice)+1)|(2<<24))
	//	binary.Write(bb, binary.LittleEndian, uint32(xe.x)|(uint32(xe.y)<<16))
	//	binary.Write(bb, binary.LittleEndian, uint32(xe.w)|(uint32(xe.h)<<16))
	binary.Write(bb, binary.LittleEndian, goSlice)

	return bb
}
