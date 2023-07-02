package g920

import (
	"fmt"
	"log"
	"github.com/sstallion/go-hid"
)

// Open g920 as HID device.
func Open(vid, pid uint64, verbose bool) *hid.Device {
	dev, err := hid.OpenFirst(uint16(vid), uint16(pid))
	if err != nil {
		log.Fatal(err)
	}

	
	if verbose {
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
	}
	return dev
}

// Return channel for reports from  g920 as HID device.
func Read(dev *hid.Device, progress bool) <-chan G920 {
	gs := make(chan G920)
	go func() {
		defer close(gs)
		buf := make([]byte, 65535)

		for {
			n, err := dev.Read(buf)
			if err != nil {
				log.Fatal(err)
			}

			gs <- AsG920(buf[:n])
			if progress {
				fmt.Printf("g")
			}
		}
	}()
	return gs
}

