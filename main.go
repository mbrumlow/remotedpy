package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"net/http"
	"os"

	"github.com/mbrumlow/remotedpy/x11"
	"golang.org/x/net/websocket"
)

type ScreenInfo struct {
	Width  int
	Height int
}

func main() {

	http.Handle("/dpy", websocket.Handler(DpyServer))
	fs := http.FileServer(http.Dir("webroot"))
	http.Handle("/remotedpy/", http.StripPrefix("/remotedpy/", fs))

	err := http.ListenAndServe(":8888", nil)
	if err != nil {
		panic("ListenAndServe: " + err.Error())
	}
}

func DpyServer(ws *websocket.Conn) {

	dpy, err := x11.OpenDisplay()
	if err != nil {
		fmt.Println("ERR: " + err.Error())
		os.Exit(-1)
	}
	defer dpy.Close()

	w, h := dpy.GetScreenSize()
	si := &ScreenInfo{Width: w, Height: h}
	websocket.JSON.Send(ws, si)

	dpy.StartStream()
	defer dpy.StopStream()
	for img := range dpy.C {

		bb := new(bytes.Buffer)

		binary.Write(bb, binary.LittleEndian, uint32(img.X))
		binary.Write(bb, binary.LittleEndian, uint32(img.Y))
		binary.Write(bb, binary.LittleEndian, uint32(img.W))
		binary.Write(bb, binary.LittleEndian, uint32(img.H))

		for y := 0; y < img.H; y++ {
			for x := 0; x < img.W; x++ {
				p := img.GetPixel(x, y)
				binary.Write(bb, binary.LittleEndian, p)
			}
		}

		err = websocket.Message.Send(ws, bb.Bytes())
		if err != nil {
			fmt.Println("ENDING: " + err.Error())
			return
		}

		img.DistoryImage()
	}
}
