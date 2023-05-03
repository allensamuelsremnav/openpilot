// Package net handles duplication and deduplication of modem channels.
package net

import (
	"log"
	"net"
	"sync"
)

// udpDev sends msgs over a named device to dest.
func udpDev(msgs <-chan []byte, device string, dest string, startedWG *sync.WaitGroup, completedWG *sync.WaitGroup, verbose bool) error {
	defer completedWG.Done()
	conn, err := DialUDP(device, dest, "UDPSendDev")
	if err != nil {
		log.Fatal(err)
	}

	go func() {
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
	}()
	return nil
}

// UDPDup sends duplicated messages over named devices to dest.
func UDPDup(msgs <-chan []byte, devices []string, dest string, verbose bool) {
	// Make a parallel array of channels and goroutines.
	// This WaitGroup to be sure the senders are ready to receive.
	var startedWG sync.WaitGroup

	// This WaitGroup for all senders to complete.
	var completedWG sync.WaitGroup

	var chs []chan []byte
	startedWG.Add(len(devices))
	completedWG.Add(len(devices))

	for _, d := range devices {
		ch := make(chan []byte)
		chs = append(chs, ch)
		udpDev(ch, d, dest, &startedWG, &completedWG, verbose)
	}
	startedWG.Wait()

	// Send the msgs to the parallel channels.
	for msg := range msgs {
		for j := 0; j < len(devices); j++ {
			select {
			case chs[j] <- msg:
			default:
				log.Printf("%s channel not ready, packet dropped",
					devices[j])
			}
		}
	}

	for _, ch := range chs {
		close(ch)
	}
	completedWG.Wait()
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
