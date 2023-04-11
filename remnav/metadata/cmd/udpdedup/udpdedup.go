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
	port := flag.String("addr", "", "port for UDP, e.g. 6007")
	flag.Parse()
	progName := filepath.Base(os.Args[0])

	// listen to incoming udp packets
	fullport := fmt.Sprintf(":%s", *port)
	log.Printf("%s: port %s\n", progName, fullport)
	pc, err := net.ListenPacket("udp", fullport)
	if err != nil {
		log.Fatal(err)
	}
	defer pc.Close()

	for {
		buf := make([]byte, 1024)
		n, addr, err := pc.ReadFrom(buf)
		if err != nil {
			continue
		}
		fmt.Printf("%s %d %s", addr, n, string(buf[:n]))
	}

}
