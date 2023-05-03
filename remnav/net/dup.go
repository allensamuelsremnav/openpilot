// Package net handles duplication and deduplication of modem channels.
package net

import (
	"log"
	"net"
	"sync"
)

// udpDev sends msgs over a PacketConn
func udpDev(msgs <-chan []byte, device string, conn *net.UDPConn, startedWG *sync.WaitGroup, completedWG *sync.WaitGroup, verbose bool) {
	defer completedWG.Done()
	startedWG.Done()

	for msg := range msgs {
		_, err := conn.Write([]byte(msg))
		if err != nil {
			log.Printf("UDPSendDev: %v", err)
		}
		if verbose {
			log.Printf("UDPSendDev: device %s, msg %s\n", device, string(msg))
		}
	}
}

// UDPDup sends duplicated messages over named devices to dest.
func UDPDup(msgs <-chan []byte, devices []string, dest string, completedWG *sync.WaitGroup, verbose bool) {
	tag := "UDPDup"
	// Make a parallel array of channels and goroutines.

	// Use this WaitGroup to be sure the senders are ready to receive
	// since later on we will use non-blocking writes to the input
	// channels, and we want to distinguish channel-not-ready from
	// goroutine "not started" and "busy handling previous message"
	var startedWG sync.WaitGroup

	var chs []chan []byte
	// We might not use some of the devices.
	var activeDevices []string
	for _, d := range devices {
		conn, err := DialUDP(d, dest, "UDPSendDev")
		if err != nil {
			log.Println(err)
			completedWG.Done()
			continue
		}
		ch := make(chan []byte)
		startedWG.Add(1)
		go udpDev(ch, d, conn, &startedWG, completedWG, verbose)
		chs = append(chs, ch)
		activeDevices = append(activeDevices, d)
	}
	startedWG.Wait()

	if len(chs) == 0 {
		return
	}
	log.Printf("%s: active devices %v", tag, activeDevices)

	// Send the msgs to the parallel channels.
	go func() {
		for msg := range msgs {
			for j := 0; j < len(chs); j++ {
				select {
				case chs[j] <- msg:
				default:
					log.Printf("%s channel not ready, packet dropped",
						activeDevices[j])
				}
			}
		}

		for _, ch := range chs {
			close(ch)
		}
	}()
}

// dup sends duplicated messages over network to destinations
func dup(network string, msgs <-chan []byte, dests []string, verbose bool) {
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
