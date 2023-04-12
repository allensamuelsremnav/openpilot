// Package net handles duplication and deduplication of modem channels.
package net

import (
	"log"
	"net"
	"sync"
)

// UDPSendDev sends msgs over a named device to dest.
func UDPSendDev(msgs <-chan string, device string, dest string, startedWG *sync.WaitGroup, completedWG *sync.WaitGroup, verbose bool) {
	defer completedWG.Done()
	// Initialize the dialer for an explicit device.
	ibn, err := net.InterfaceByName(device)
	if err != nil {
		log.Fatal(err)
	}
	iaddrs, err := ibn.Addrs()
	if err != nil {
		log.Fatal(err)
	}
	if len(iaddrs) == 0 {
		log.Fatalf("%s had no interface addresses (%d)", device, len(iaddrs))
	}
	udpAddr := &net.UDPAddr{
		IP: iaddrs[0].(*net.IPNet).IP,
	}
	log.Printf("%s udpAddr: %v\n", device, udpAddr)
	dialer := net.Dialer{LocalAddr: udpAddr}

	conn, err := dialer.Dial("udp", dest)
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()

	startedWG.Done()
	for msg := range msgs {
		_, err := conn.Write([]byte(msg))
		if err != nil {
			log.Print(err)
		}
		if verbose {
			log.Printf("device %s, msg '%s'\n", device, msg)
		}
	}
}

// UDPDupDev sends duplicated messages over named devices to dest.
func UDPDupDev(msgs <-chan string, devices []string, dest string, verbose bool) {
	// Make a parallel array of channels and goroutines.
	// This WaitGroup to be sure the senders are ready to receive.
	var startedWG sync.WaitGroup
	// This WaitGroup for all senders to complete.
	var completedWG sync.WaitGroup

	var chs []chan string
	startedWG.Add(len(devices))
	completedWG.Add(len(devices))
	for _, d := range devices {
		ch := make(chan string)
		chs = append(chs, ch)
		go UDPSendDev(ch, d, dest, &startedWG, &completedWG, verbose)
	}
	startedWG.Wait()

	for msg := range msgs {
		for j := 0; j < len(devices); j++ {
			select {
			case chs[j] <- msg:
			default:
				log.Printf("unable to send packet to %s", devices[j])
			}
		}
	}

	for _, ch := range chs {
		close(ch)
	}
	completedWG.Wait()
}

// dup sends duplicated messages over network to destinations
func dup(network string, msgs <-chan string, dests []string, verbose bool) {
	var conns []net.Conn
	for _, d := range dests {
		conn, err := net.Dial(network, d)
		if err != nil {
			log.Fatal(err)
		}
		conns = append(conns, conn)
		defer conn.Close()
	}
	for msg := range msgs {
		for _, conn := range conns {
			i, err := conn.Write([]byte(msg))
			if err != nil {
				log.Print(err)
			}
			if verbose {
				log.Printf("dest %s, msg '%s'\n", dests[i], msg)
			}
		}
	}
}

// UDPDup sends duplicated messages over network to destinations
func UDPDup(msgs <-chan string, dests []string, verbose bool) {
	dup("udp", msgs, dests, verbose)
}
