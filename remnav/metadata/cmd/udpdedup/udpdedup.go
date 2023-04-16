// Application to debug UDP applications by listening on a port.
package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"path/filepath"
)

func main() {
	port := flag.String("port", "", "listen on this port for UDP, e.g. 6001")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	flag.Parse()
	progName := filepath.Base(os.Args[0])

	// listen to incoming udp packets
	log.Printf("%s: port %s\n", progName, *port)
	pc, err := net.ListenPacket("udp", ":" + *port)
	if err != nil {
		log.Fatal(err)
	}
	defer pc.Close()

	for {
		buf := make([]byte, *bufSize)
		n, addr, err := pc.ReadFrom(buf)
		if err != nil {
			log.Fatal(err)
		}
		fmt.Printf("%s %d %s\n", addr, n, string(buf[:n]))
	}

}
