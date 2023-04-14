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
	"sync"

	rnnet "remnav.com/remnav/net"
)

func main() {
	port := flag.Int("port", 6001, "listen on this port for UDP, e.g. 6001")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	flag.Parse()
	progName := filepath.Base(os.Args[0])
	log.Printf("%s: port %d\n", progName, *port)

	// listen to incoming udp packets
	pc, err := net.ListenPacket("udp", ":"+strconv.Itoa(*port))
	if err != nil {
		log.Fatal(err)
	}
	defer pc.Close()

	msgs, addrs := rnnet.Chan(pc, *bufSize)

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for msg := range msgs {
			fmt.Println(string(msg))
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		for addr := range addrs {
			fmt.Printf("ReadFrom %s", addr)
		}
	}()
	wg.Wait()
}
