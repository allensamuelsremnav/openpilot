package net

import (
	"errors"
	"fmt"
	"log"
	"net"
)

func DialUDP(device, dest, tag string) (*net.UDPConn, error) {
	// Initialize the dialer for an explicit device.
	ibn, err := net.InterfaceByName(device)
	if err != nil {
		return nil, errors.New(fmt.Sprintf("%s: %v '%s'", tag, err, device))
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
	remoteAddr, err := net.ResolveUDPAddr("udp", dest)
	if err != nil {
		return nil, errors.New(fmt.Sprintf("%s: %s ResolveUDPAddr failed: %v", tag, dest, err))
	}
	log.Printf("%s: %s localAddr: %v\n", tag, device, localAddr)
	log.Printf("%s: remoteAddr: %v\n", tag, remoteAddr)

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
