// Application to debug UDP applications by listening on a port.
package main

// ncat apparently doesn't do well if there are two programs sending
// to the port, either simultaneously or sequentially.

import (
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"

	rnnet "remnav.com/remnav/net"
)

func main() {
	port := flag.Int("port", 6001, "listen on this port for UDP, e.g. 6001")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")

	flag.Parse()
	progName := filepath.Base(os.Args[0])
	log.Printf("%s: port %d\n", progName, *port)

	var send chan []byte
	recvd := rnnet.BidiRW(*port, *bufSize, send)

	for msg := range recvd {
		fmt.Printf("%d bytes, %s\n", len(msg), string(msg))
		send <- []byte("echo: " + string(msg))
	}

}
