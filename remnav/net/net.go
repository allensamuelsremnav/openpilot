package net

import (
	"log"
	"net"
)

func DialUDP(device, dest, tag string) *net.UDPConn {
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
		log.Fatalf("%s: %s had no interface addresses (%d)", tag, device, len(iaddrs))
	}
	localAddr := &net.UDPAddr{
		IP: iaddrs[0].(*net.IPNet).IP,
	}
	remoteAddr, err := net.ResolveUDPAddr("udp", dest)
	if err != nil {
		log.Fatalf("%s: %s ResolveUDPAddr failed: %v", tag, dest, err)
	}
	log.Printf("%s: %s localAddr: %v\n", tag, device, localAddr)
	log.Printf("%s: remoteAddr: %v\n", tag, remoteAddr)

	var pc *net.UDPConn
	pc, err = net.DialUDP("udp", localAddr, remoteAddr)
	if err != nil {
		log.Fatal(err)
	}
	return pc
}
