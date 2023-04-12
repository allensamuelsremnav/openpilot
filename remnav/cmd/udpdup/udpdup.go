// Test program for sending duplicate data over modems.
package main

import (
	"flag"
	"log"
	"os"
	"strconv"
	"time"

	udp "remnav.com/remnav/net"
)

func main() {
	dest := flag.String("dest", "10.0.0.11:6001", "destination address")
	verbose := flag.Bool("verbose", false, "verbosity on")
	packets := flag.Int("packets", 100, "number of test packets")
	flag.Parse()
	devices := flag.Args()
	if len(devices) == 0 {
		devices = []string{"eth0", "wlan0"}
	}
	log.Printf("%s, destination %s", os.Args[0], *dest)
	log.Printf("%s, devices %v", os.Args[0], devices)

	msgs := make(chan []byte)
	go udp.UDPDupDev(msgs, devices, *dest, *verbose)

	for i := 0; i < *packets; i++ {
		msgs <- []byte(strconv.Itoa(i))
		time.Sleep(1 * time.Second)
	}
	close(msgs)
}
