package net

import (
	"fmt"
	"log"
	"net"
	"strconv"
	"sync"
)

// These functions implement bidirectional communication over
// replicated UDP connections.

// BidiWR (BidiRW) implements the dialer (listener) end of dialer -->
// listener, with back communications dialer <-- listener

// These can be used for unidirectional dialer -> listener by not
// sending on the listener back channel and not reading on the dialer
// back channel.

// bidiWRDev implements dialing on a single devices, sending over
// the connnection, and reading back communications.  It sends
// until the send-message channel is closed and reports
// sendWG.Done() when the send-message channel is closed.  Back
// communications are forwarded to the returned chan<-[]byte, which is
// never closed, without deduping.
func bidiWRDev(send <-chan []byte, device string, deviceId uint8, pc *net.UDPConn, bufSize int, sendWG *sync.WaitGroup, verbose bool) <-chan []byte {
	// bufSize should be big enough for back communication.
	log.Printf("bidiWRDev: device %s, deviceId %d\n", device, deviceId)

	// Send msgs to connnection.
	go func() {
		defer sendWG.Done()
		for msg := range send {
			// dialer->listener msgs are prefixed with a byte containing the logical device id.
			_, err := pc.Write(append([]byte{deviceId}, msg...))
			if err != nil {
				log.Printf("bidiWRDev: %v", err)
				continue
			}
			if verbose {
				log.Printf("bidiWRDev: ->%s, %d bytes, '%s'\n", device, len(msg), msg)
			}
		}
	}()

	recvChan := make(chan []byte)
	// Read and forward.
	go func() {
		for {
			buf := make([]byte, bufSize)
			n, _, err := pc.ReadFrom(buf)
			if err != nil {
				log.Printf("bidiWRDev/ReadFrom: %v", err)
				continue
			}
			recvChan <- buf[:n]
		}
	}()
	return recvChan
}

// BidiWR implements a virtual connection using multiple devices,
// sending messages over all devices and collecting all back
// communication.
func BidiWR(sendMsgs <-chan []byte, devices []string, dest string, bufSize int, wg *sync.WaitGroup, verbose bool) <-chan []byte {
	// Make a parallel array of channels and goroutines.

	var sendChans []chan []byte
	var recvChs []<-chan []byte
	var activeDevices []string
	for i, d := range devices {
		pc, err := DialUDP(d, dest, "BidiWR")
		if err != nil {
			log.Println(err)
			continue
		}
		activeDevices = append(activeDevices, d)
		sCh := make(chan []byte)
		sendChans = append(sendChans, sCh)
		wg.Add(1)
		rCh := bidiWRDev(sCh, d, uint8(i), pc, bufSize, wg, verbose)
		recvChs = append(recvChs, rCh)
	}

	// Send to all devices.
	go func() {
		for msg := range sendMsgs {
			for j := 0; j < len(sendChans); j++ {
				select {
				case sendChans[j] <- msg:
				default:
					log.Printf("BidiWR: %s channel not ready, packet dropped",
						activeDevices[j])
				}
			}
		}
		for _, ch := range sendChans {
			close(ch)
		}
	}()

	// Merge recvChs to a single channel.
	recvChan := make(chan []byte)

	var recvWG sync.WaitGroup
	for _, ch := range recvChs {
		recvWG.Add(1)
		go func(c <-chan []byte) {
			for msg := range c {
				recvChan <- msg
			}
			recvWG.Done()
		}(ch)
	}
	// Close merged channel when all recvChs are closed.
	go func() {
		recvWG.Wait()
		close(recvChan)
	}()

	return recvChan
}

type bidiSource struct {
	logical uint8
	addr    net.Addr
}

// BidiRW implements listening to a port (without removing
// duplicates), forwarding to its return value (a channel), and
// sending messages to all ReadFrom addresses.
func BidiRW(port int, bufSize int, send <-chan []byte, verbose bool) <-chan []byte {
	recvd := make(chan []byte)
	addrs := make(chan bidiSource)

	pc, err := net.ListenPacket("udp", ":"+strconv.Itoa(port))
	if err != nil {
		log.Fatal(err)
	}

	// Forward messages from port
	go func() {
		for {
			buf := make([]byte, bufSize)
			n, addr, err := pc.ReadFrom(buf)
			if err != nil {
				log.Println(err)
				continue
			}

			// dialer->listener messages are prefixed with a byte containing the logical device id.
			msg := buf[1:n]
			deviceId := uint8(buf[0])
			recvd <- msg
			addrs <- bidiSource{logical: deviceId, addr: addr}
			if verbose {
				fmt.Printf("BidiRW (ReadFrom), device %d, %d bytes, %s\n", deviceId, n, string(msg))
			}
		}
	}()

	// Forward messages from send channel; maintain dictionary of ReadFrom addresses.
	go func() {
		var addrs = addrs
		sources := make(map[uint8]bidiSource)
		var send = send
		for {
			select {
			case addr, ok := <-addrs:
				if !ok {
					addrs = nil
					break
				}
				k := addr.logical
				if _, ok := sources[k]; !ok {
					log.Printf("BidiRW: added sources[%d] with %v\n", k, addr.addr)
				}
				sources[k] = addr
			case msg, ok := <-send:
				if !ok {
					send = nil
					break
				}
				for k, v := range sources {
					if verbose {
						log.Printf("BidiRW (send) device %d, %v\n", k, v.addr)
					}
					_, err := pc.WriteTo(msg, v.addr)
					if err != nil {
						log.Printf("BidiWR: (send) device %d, %v, %v", k, v, err)
						continue
					}
				}
			}
		}
	}()

	return recvd

}
