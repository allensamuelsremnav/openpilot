package net

import (
	"errors"
	"fmt"
	"log"
	"net"
	"strconv"
	"sync"
)

func DialUDP(device, dest, tag string) (*net.UDPConn, error) {
	remoteAddr, err := net.ResolveUDPAddr("udp", dest)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("%s: remoteAddr: %v\n", tag, remoteAddr)

	// Initialize the dialer for an explicit device.
	ibn, err := net.InterfaceByName(device)
	if err != nil {
		log.Fatalf("%s: %v '%s'", tag, err, device)
	}

	iaddrs, err := ibn.Addrs()
	if err != nil {
		return nil, err
	}
	if len(iaddrs) == 0 {
		return nil, errors.New(fmt.Sprintf("%s: %s had no interface addresses (%d)", tag, device, len(iaddrs)))
	}
	localAddr := &net.UDPAddr{
		IP: iaddrs[0].(*net.IPNet).IP,
	}
	log.Printf("%s: %s localAddr: %v\n", tag, device, localAddr)

	var pc *net.UDPConn
	pc, err = net.DialUDP("udp", localAddr, remoteAddr)
	if err != nil {
		return nil, err
	}
	return pc, nil
}

// Make channels for the packets and addrs from a PacketConn.ReadFrom.
func ReadFrom(pc net.PacketConn, bufSize int) (<-chan []byte, chan net.Addr) {
	msgs := make(chan []byte)
	addrs := make(chan net.Addr)
	go func() {
		for {
			buf := make([]byte, bufSize)
			n, addr, err := pc.ReadFrom(buf)
			if err != nil {
				log.Print(err)
				break
			}
			msgs <- buf[:n]
			addrs <- addr
		}
		close(msgs)
		close(addrs)
	}()
	return msgs, addrs
}

// Write msgs to this port.
func WritePort(msgs chan []byte, port int, wg *sync.WaitGroup) {
	defer wg.Done()
	conn, err := net.Dial("udp", ":"+strconv.Itoa(port))
	defer conn.Close()
	if err != nil {
		log.Fatal(err)
	}
	for msg := range msgs {
		_, err := conn.Write(msg)
		if err != nil {
			log.Printf("port %d: %v\n", port, err)
		}
	}
}
