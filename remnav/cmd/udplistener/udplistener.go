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

func channels(pc net.PacketConn, bufSize int) {
	msgs, addrs := rnnet.Chan(pc, bufSize)

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
		seen := make(map[string]bool)
		for addr := range addrs {
			_, ok := seen[addr.String()]
			if !ok {
				fmt.Printf("ReadFrom %s\n", addr)
				seen[addr.String()] = true
				fmt.Print(seen)
			}
		}
	}()
	wg.Wait()
}

func main() {
	port := flag.Int("port", 6001, "listen on this port for UDP, e.g. 6001")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	// one of these
	chans := flag.Bool("chans", false, "show output of remnav/net/Chan")
	raw := flag.Bool("raw", false, "show output of connnection (as strings)")

	flag.Parse()
	progName := filepath.Base(os.Args[0])
	log.Printf("%s: port %d\n", progName, *port)

	// listen to incoming udp packets
	pc, err := net.ListenPacket("udp", ":"+strconv.Itoa(*port))
	if err != nil {
		log.Fatal(err)
	}
	defer pc.Close()

	if *chans {
		channels(pc, *bufSize)
	} else if *raw {
		buf := make([]byte, *bufSize)
		for {
			n, addr, err := pc.ReadFrom(buf)
			if err != nil {
				log.Fatal(err)
			}
			fmt.Printf("%s: %d bytes, %s\n", addr.String(), n, string(buf[:n]))
		}
	} else {
		log.Fatal("need --chans or --raw")
	}
}
