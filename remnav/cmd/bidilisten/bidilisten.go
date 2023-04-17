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
	"strconv"
	"sync"
	"time"

	rnnet "remnav.com/remnav/net"
)

func main() {
	port := flag.Int("port", 6001, "listen on this port for UDP, e.g. 6001")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	sleep := flag.Int("sleep", 1000, "sleep between packets, microseconds")
	flag.Parse()

	sleepDuration := time.Duration(*sleep) * time.Microsecond

	progName := filepath.Base(os.Args[0])
	log.Printf("%s: port %d\n", progName, *port)

	send := make(chan []byte)
	recvd := rnnet.BidiRW(*port, *bufSize, send)

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for msg := range recvd {
			fmt.Printf("BidiListen (recvd) %d bytes, %s #400\n", len(msg), string(msg))
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		for i := 0; i < 100; i++ {
			send <- []byte("echo " + strconv.Itoa(i))
			time.Sleep(sleepDuration)
		}
	}()
	wg.Wait()
}
