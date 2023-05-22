// Application to debug reading and decoding G920 messages.
package main

import (
	"flag"
	"fmt"
	"log"
	"strconv"
	"strings"

	"github.com/sstallion/go-hid"
	"remnav.com/remnav/g920"
)

func main() {
	vidPID := flag.String("vidpid", "046d:c262", "colon-separated hex vid and pid")
	flag.Parse()

	ids := strings.Split(*vidPID, ":")
	vid, err := strconv.ParseUint(ids[0], 16, 16)
	if err != nil {
		log.Fatal(err)
	}
	pid, err := strconv.ParseUint(ids[1], 16, 16)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("vid:pid %04x:%04x %d:%d\n", vid, pid, vid, pid)

	dev, err := hid.OpenFirst(uint16(vid), uint16(pid))
	if err != nil {
		log.Fatal(err)
	}

	info, err := dev.GetDeviceInfo()
	if err != nil {
		log.Fatal(err)
	}

	log.Printf("%s: ID %04x:%04x %s %s\n",
		info.Path,
		info.VendorID,
		info.ProductID,
		info.MfrStr,
		info.ProductStr)

	// g920 has only one indexed string?
	for i := 0; i < 10; i++ {
		s, err := dev.GetIndexedStr(1)
		if err != nil {
			fmt.Printf("%d: %v\n", i, err)
		}
		if s != "Logitech" {
			log.Printf("unexpected indexed string %d: %s\n", i, s)
		}
	}

	// Counters to estimate how often the pedal states change.
	var allCount, pedalCount int
	pedalLeft := -1
	pedalMiddle := -1
	pedalRight := -1

	for {
		buf := make([]byte, 65535)
		n, err := dev.Read(buf)
		if err != nil {
			log.Fatal(err)
		}
		d, err := g920.Decode(buf[:n])
		if err != nil {
			log.Fatal(err)
		}
		allCount += 1
		if d.PedalLeft != pedalLeft || d.PedalMiddle != pedalMiddle || d.PedalRight != pedalRight {
			pedalCount += 1
			pedalLeft = d.PedalLeft
			pedalMiddle = d.PedalMiddle
			pedalRight = d.PedalRight
		}
		if allCount%1000 == 0 {
			fmt.Printf("pedalCount %d/%d, %.2f\n", pedalCount, allCount, float64(pedalCount)/float64(allCount))
			allCount = 0
			pedalCount = 0
			pedalLeft = 0
			pedalMiddle = 0
			pedalRight = 0
		}
		fmt.Printf("wheel %6d (%d, %d), pedal (%3d, %3d, %3d), dpad_xboxabxy %3d, buttons_flappy %3d\n",
			d.Wheel-256*128, buf[g920.WheelHighByte], buf[g920.WheelLowByte], d.PedalLeft, d.PedalMiddle, d.PedalRight, d.DpadXboxABXY, d.ButtonsFlappy)
		fmt.Println(g920.AsG920(buf[:n]))
	}
}
