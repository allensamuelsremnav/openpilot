package main

import (
	"flag"
	"fmt"
	"log"
	"strconv"
	"strings"

	"github.com/sstallion/go-hid"
)

func main() {
	vidPID := flag.String("vidpid", "046d:c261", "colon-separated hex vid and pid")
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
	for i := 1; i < 2; i++ {
		s, err := dev.GetIndexedStr(1)
		if err != nil {
			fmt.Printf("%d: %v\n", i, err)
		}
		fmt.Printf("%d: %s\n", i, s)
	}

	for {
		buf := make([]byte, 65535)
		n, err := dev.GetFeatureReport(buf)
		if err != nil {
			log.Fatal(err)
		}
		fmt.Println(buf[:n])
	}
}
