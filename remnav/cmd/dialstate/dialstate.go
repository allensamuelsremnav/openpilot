// Study whether ipaddress appear or disappear for a named network interface.
package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"strconv"
	"strings"
	"sync"
	"time"

	rnnet "remnav.com/remnav/net"
)

type State struct {
	ibn       *net.Interface
	iaddrs    []net.Addr
	localAddr string
}

func monitor(device, dest string, sleep time.Duration, wg *sync.WaitGroup) {
	defer wg.Done()
	var state State
	loop := func() {
		_, err := net.ResolveUDPAddr("udp", dest)
		if err != nil {
			log.Printf("%s: %v\n", device, err)
			return
		}

		// Initialize the dialer for an explicit device.
		ibn, err := net.InterfaceByName(device)
		if err != nil {
			log.Printf("%s: %v\n", device, err)
			return
		}

		if state.ibn == nil {
			state.ibn = ibn
			log.Printf("%s: ibn nil --> not nil\n", device)
			iaddrs, err := ibn.Addrs()
			if err != nil {
				log.Printf("%s: %v\n", device, err)
			} else {
				log.Printf("%s: %d addresses for interface\n", device, len(iaddrs))
			}
			state.iaddrs = make([]net.Addr, len(iaddrs))
			copy(state.iaddrs, iaddrs)
			if len(iaddrs) > 0 {
				localAddr := &net.UDPAddr{
					IP: iaddrs[0].(*net.IPNet).IP,
				}
				state.localAddr = fmt.Sprintf("%v", localAddr)
			} else {
				state.localAddr = ""
			}
			log.Printf("%s: state.localAddr %s\n", device, state.localAddr)
			return
		}

		iaddrs, err := ibn.Addrs()
		if err != nil {
			log.Printf("%s: %v\n", device, err)
		}

		var localAsString string
		if len(iaddrs) > 0 {
			localAddr := &net.UDPAddr{
				IP: iaddrs[0].(*net.IPNet).IP,
			}
			localAsString = fmt.Sprintf("%v", localAddr)
		}

		if len(state.iaddrs) != len(iaddrs) {
			log.Printf("%s: len(iaddrs) %d --> %d\n", device, len(state.iaddrs), len(iaddrs))
			state.iaddrs = make([]net.Addr, len(iaddrs))
			copy(state.iaddrs, iaddrs)
			return
		}
		if state.localAddr != localAsString {
			log.Printf("%s: localAddr '%s' --> '%s'\n", device,
				state.localAddr, localAsString)
			state.localAddr = localAsString
			state.iaddrs = make([]net.Addr, len(iaddrs))
			copy(state.iaddrs, iaddrs)
			return
		}
		// hole in logic: lengths are the same but something in the tail has changed.
	}
	for {
		log.Println("checking", device)
		loop()
		time.Sleep(sleep)
	}
}
func main() {
	dest := flag.String("dest",
		"10.0.0.60:"+strconv.Itoa(rnnet.OperatorGpsdListen),
		"destination host:port, e.g. 96.64.247.70:"+strconv.Itoa(rnnet.OperatorGpsdListen))
	devs := flag.String("devices", "eth0,eth0",
		"comma-separated list of network devices, e.g. wlan0_1,wlan1_1,wlan2_1")
	sleep := flag.Int("sleep", 10, "sleep interval in seconds between checks")

	flag.Parse()
	var devices []string
	for _, t := range strings.Split(*devs, ",") {
		if len(t) == 0 {
			continue
		}
		devices = append(devices, t)
	}

	var wg sync.WaitGroup
	wg.Add(len(devices))
	for _, d := range devices {
		go monitor(d, *dest, time.Duration(*sleep)*time.Second, &wg)
	}
	wg.Wait()
}
