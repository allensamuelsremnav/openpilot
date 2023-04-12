// Application to debug UDP applications by listening on a port.
package main

// ncat apparently doesn't do well if there are two programs sending
// to the port, either simultaneously or sequentially.

import (
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"path/filepath"
	"strconv"
)

func main() {
	port := flag.Int("port", 6001, "listen on this port for UDP, e.g. 6001")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	flag.Parse()
	progName := filepath.Base(os.Args[0])

	// listen to incoming udp packets
	log.Printf("%s: port %d\n", progName, *port)
	pc, err := net.ListenPacket("udp", ":"+strconv.Itoa(*port))
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
