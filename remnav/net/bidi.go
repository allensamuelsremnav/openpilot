package net

import (
	"fmt"
	"log"
	"net"
	"os"
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

// BidiWRDev implements a dialing on a single devices, sending over
// the connnection, and reading back communications.  It sends
// until the send-message channel is closed and reports
// sendWG.Done() when the send-message channel is closed.  Back
// communications are forwarded to the returned chan<-[]byte, which is
// never closed, without deduping.
func BidiWRDev(send <-chan []byte, device string, deviceId uint8, dest string, bufSize int, sendWG *sync.WaitGroup, recvChan chan<- []byte, verbose bool) {
	// bufSize should be big enough for back communication.
	log.Printf("BidiWRDev: device %s, deviceId %d\n", device, deviceId)

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

	// Send msgs to connnection.
	sendWG.Add(1)
	go func() {
		defer sendWG.Done()
		defer pc.Close()
		for msg := range send {
			// dialer->listener msgs are prefixed with a byte containing the logical device id.
			_, err := pc.Write(append([]byte{deviceId}, msg...))
			if err != nil {
				log.Printf("BidiWRDev: %v", err)
				continue
			}
			if verbose {
				log.Printf("BidiWRDev: ->%s, %d bytes, '%s'\n", device, len(msg), msg)
			}
		}
	}()

	// Read and forward.
	go func() {
		for {
			buf := make([]byte, bufSize)
			n, _, err := pc.ReadFrom(buf)
			if err != nil {
				log.Printf("BidiWRDev/ReadFrom %v", err)
				continue
			}
			recvChan <- buf[:n]
		}
	}()
}

// BidiWR implements a virtual connection using multiple devices,
// sending messages over all devices and collecting all back
// communication.
func BidiWR(sendMsgs <-chan []byte, devices []string, dest string, bufSize int, verbose bool) <-chan []byte {
	// Make a parallel array of channels and goroutines.
	// This WaitGroup to be sure the senders are ready to receive.

	// This WaitGroup for all senders to complete.
	var sendWG sync.WaitGroup

	var sendChans []chan []byte
	var recvChan chan []byte = make(chan []byte)

	for i, d := range devices {
		ch := make(chan []byte)
		sendChans = append(sendChans, ch)
		BidiWRDev(ch, d, uint8(i), dest, bufSize, &sendWG, recvChan, verbose)
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

	go func() {
		defer pc.Close()
		defer close(recvd)
		defer close(addrs)
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

	// Each logical source has a bidiSource.
	sources := make(map[uint8]bidiSource)

	// Forward send messages to the connection.
	var sendWrite = func() {
		go func() {
			for msg := range send {
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
		}()
	}

	// Maintain the dictionary of device ids --> address
	go func() {
		var addrs = addrs
		// Start sending only after we've received a packet
		// and have a return address.
		var startSend sync.Once
		for {
			select {
			case addr, ok := <-addrs:
				if !ok {
					fmt.Printf("BidiRW: addrs = nil\n")
					addrs = nil
				} else {
					k := addr.logical
					v, ok := sources[k]
					if !ok {
						sources[k] = addr
						log.Printf("BidiRW: added sources[%d] with %v\n", k, addr.addr.String())
					} else if k != v.logical {
						sources[k] = addr
						log.Printf("BidiRW: replaced sources[%d] with %v\n", k, addr.addr.String())
					}
					startSend.Do(sendWrite)
				}
			}
		}
	}()

	return recvd

}
