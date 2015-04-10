package main

import (
	"encoding/json"
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

const (
	KeyEventType = 1 << iota
	MoseMoveType
	MouseClickType
)

type Event struct {
	Type  int
	Event string
}

type KeyEvent struct {
	KeyCode uint32
	Down    bool
}

type MoseMoveEvent struct {
	X uint32
	Y uint32
}

type MouseClickEvent struct {
	Button int
	X      uint32
	Y      uint32
	Down   bool
}

type ScreenInfo struct {
	Width  int
	Height int
}

func main() {

	//	go timeTick()

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

func handleEvents(ws *websocket.Conn, dpy *x11.Display) {
	for {
		var ev Event
		err := websocket.JSON.Receive(ws, &ev)
		if err != nil {
			break
		}

		switch ev.Type {
		case KeyEventType:
			var e KeyEvent
			err := json.Unmarshal([]byte(ev.Event), &e)
			if err != nil {
				fmt.Println("Error on event: " + err.Error())
				break
			}
			dpy.SendKey(e.KeyCode, e.Down)
			break
		case MoseMoveType:
			var e MoseMoveEvent
			err := json.Unmarshal([]byte(ev.Event), &e)
			if err != nil {
				fmt.Println("Error on mouse move event: " + err.Error())
				break
			}
			dpy.SendMouseMove(e.X, e.Y)
			break
		case MouseClickType:
			var e MouseClickEvent
			err := json.Unmarshal([]byte(ev.Event), &e)
			if err != nil {
				fmt.Println("Error on mouse click event: " + err.Error())
				break
			}
			dpy.SendMouseClick(e.Button, e.X, e.Y, e.Down)
			break
		default:
			fmt.Printf("Unknown event: %v\n", ev)
		}
	}
}

func DpyServer(ws *websocket.Conn) {

	dpy, err := x11.OpenDisplay()
	if err != nil {
		fmt.Println("ERR: " + err.Error())
		os.Exit(-1)
	}
	defer dpy.Close()

	go handleEvents(ws, dpy)

	w, h := dpy.GetScreenSize()
	si := &ScreenInfo{Width: w, Height: h}
	websocket.JSON.Send(ws, si)

	//sw := false

	dpy.StartStream()
	defer dpy.StopStream()

	for s := range dpy.S {

		err = websocket.Message.Send(ws, s.Bytes())
		if err != nil {
			fmt.Println("ENDING: " + err.Error())
			return
		}

		mu.Lock()
		count++
		mu.Unlock()
	}

	/*
		for img := range dpy.C {

			bb := new(bytes.Buffer)

			// example screen res info
			w, h := dpy.GetScreenSize()
			binary.Write(bb, binary.LittleEndian, uint32(1|(1<<24)))
			binary.Write(bb, binary.LittleEndian, uint32(w|h<<16))

			if !sw {

				// example new window
				binary.Write(bb, binary.LittleEndian, uint32(3|(2<<24)))
				binary.Write(bb, binary.LittleEndian, uint32(5353))
				binary.Write(bb, binary.LittleEndian, uint32(0|0<<16))
				binary.Write(bb, binary.LittleEndian, uint32(565|396<<16))
				sw = true
			}

			// example window update.
			binary.Write(bb, binary.LittleEndian, uint32(len(img.S)+3)|(3<<24))
			binary.Write(bb, binary.LittleEndian, uint32(5353))
			binary.Write(bb, binary.LittleEndian, uint32(img.X)|(uint32(img.Y)<<16))
			binary.Write(bb, binary.LittleEndian, uint32(img.W)|(uint32(img.H)<<16))
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
	*/
}
