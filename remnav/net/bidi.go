// Package net handles duplication and deduplication of modem channels.
package net

import (
	"fmt"
	"log"
	"net"
	"os"
	"strconv"
	"sync"
)

// BidiWRDev a) sends msgs over a named device to dest and b) reads messages from the dest.
// Reports sendWaitGroup.Done() when it detects send-message channel is closed.
// Reads until the send-message channel is closed.
// Read messages are forwarded to the returned channel, which is never closed.
func BidiWRDev(send <-chan []byte, device string, dest string, bufSize int, sendWG *sync.WaitGroup, verbose bool) chan []byte {
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
	localAddr := &net.UDPAddr{
		IP: iaddrs[0].(*net.IPNet).IP,
	}
	remoteAddr, err := net.ResolveUDPAddr("udp", dest)
	if err != nil {
		println("ResolveUDPAddr failed:", err.Error())
		os.Exit(1)
	}
	log.Printf("%s localAddr: %v\n", device, localAddr)
	log.Printf("remoteAddr: %v\n", remoteAddr)

	var pc *net.UDPConn
	pc, err = net.DialUDP("udp", localAddr, remoteAddr)
	if err != nil {
		log.Fatal(err)
	}

	// Send msgs to connnection
	sendWG.Add(1)
	go func() {
		defer sendWG.Done()
		defer pc.Close()
		for msg := range send {
			_, err := pc.Write(msg)
			if err != nil {
				log.Printf("BidiWRDev: %v", err)
			}
			if verbose {
				log.Printf("device %s, msg '%s'\n", device, msg)
			}
		}
	}()

	// Read and forward.
	received := make(chan []byte)
	go func() {
		defer close(received)
		for {
			buf := make([]byte, bufSize)
			n, _, err := pc.ReadFrom(buf)
			if err != nil {
				log.Printf("ReadFrom %v", err)
				break
			}
			fmt.Printf("ReadFrom %d bytes %s\n", n, string(buf[:n]))
			received <- buf[:n]
		}
	}()

	return received
}

// BidiSendDev sends duplicated messages over named devices to dest.
func BidiWR(sendMsgs <-chan []byte, devices []string, dest string, bufSize int, verbose bool) []chan []byte {
	// Make a parallel array of channels and goroutines.
	// This WaitGroup to be sure the senders are ready to receive.

	// This WaitGroup for all senders to complete.
	var sendWG sync.WaitGroup

	var sendChans []chan []byte
	var rcvChans []chan []byte

	for _, d := range devices {
		ch := make(chan []byte)
		sendChans = append(sendChans, ch)
		rch := BidiWRDev(ch, d, dest, bufSize, &sendWG, verbose)
		rcvChans = append(rcvChans, rch)
	}

	// Send to all devices.
	go func() {
		for msg := range sendMsgs {
			for j := 0; j < len(devices); j++ {
				select {
				case sendChans[j] <- msg:
				default:
					log.Printf("%s channel not ready, packet dropped",
						devices[j])
				}
			}
		}
		for _, ch := range sendChans {
			close(ch)
		}
		sendWG.Wait()
	}()
	return rcvChans
}

type bidiSource struct {
	logical uint8
	addr    net.Addr
}

func BidiRW(port int, bufSize int, send <-chan []byte) <-chan []byte {
	recvd := make(chan []byte)
	addrs := make(chan bidiSource)

	pc, err := net.ListenPacket("udp", ":" + strconv.Itoa(port))
	if err != nil {
		log.Fatal(err)
	}

	go func() {
		defer pc.Close()
		defer close(recvd)
		defer close(addrs)
		for {
			buf := make([]byte, bufSize)
			n, addr, err := pc.ReadFrom(buf)
			if err != nil {
				break
			}
			recvd <- buf[:n]
			addrs <- bidiSource{logical: 0, addr: addr}
			fmt.Printf("BidiRW %d, %s\n", n, string(buf[:n]))
		}
	}()
	go func() {
		// Shadow so we can assign nil
		var addrs = addrs
		var send = send
		// Each logical source has a bidiSource.
		sources := make(map[uint8]bidiSource)
		for {
			select {
			case addr, ok := <-addrs:
				if !ok {
					addrs = nil
				} else {
					sources[addr.logical] = addr
				}
			case msg, ok := <-send:
				fmt.Printf("BidiRW (send) #202\n")
				if !ok {
					send = nil
				} else {
				        fmt.Printf("BidiRW %s\n", msg)
					for _, v := range sources {
					        fmt.Printf("BidiRW (send) %v\n", v.addr)
						_, err := pc.WriteTo(msg, v.addr)
						if err != nil {
							log.Printf("BidiWR: write %v, %v", v, err)
						}
					}
				}
			}
			if addrs == nil && send == nil {
				break
			}
		}
	}()
	return recvd

}
