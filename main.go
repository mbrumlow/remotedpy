package main

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"net/http"
	"os"
	"sync"
	"time"

	"github.com/mbrumlow/remotedpy/x11"
	"golang.org/x/net/websocket"
)

var count = uint64(0)
var mu = sync.Mutex{}

type ScreenInfo struct {
	Width  int
	Height int
}

func main() {

	go timeTick()

	http.Handle("/dpy", websocket.Handler(DpyServer))
	fs := http.FileServer(http.Dir("webroot"))
	http.Handle("/remotedpy/", http.StripPrefix("/remotedpy/", fs))

	err := http.ListenAndServe(":8888", nil)
	if err != nil {
		panic("ListenAndServe: " + err.Error())
	}
}

func timeTick() {

	c := time.Tick(1 * time.Second)
	for _ = range c {
		mu.Lock()
		n := count
		count = 0
		mu.Unlock()
		fmt.Printf("FPS: %v\n", n)
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
		binary.Write(bb, binary.LittleEndian, img.S)

		err = websocket.Message.Send(ws, bb.Bytes())
		if err != nil {
			fmt.Println("ENDING: " + err.Error())
			return
		}

		img.DistoryImage()
		mu.Lock()
		count++
		mu.Unlock()
	}
}
