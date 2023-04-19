// Application to debug UDP applications by listening on a port.
package main

// ncat apparently doesn't do well if there are two programs sending
// to the port, either simultaneously or sequentially.

import (
	"flag"
	"fmt"
	"log"
	"math/rand"
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
		fmt.Printf("bidilisten: (recvd) %v\n", recvd)
		defer wg.Done()
		for msg := range recvd {
			fmt.Printf("bidilisten (recvd) %d bytes, %s #400\n", len(msg), string(msg))
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()

		r := rand.New(rand.NewSource(time.Now().UnixNano()))
		var runes = []rune("ABCDEFGHIJKLMNOPQRSTUVWXYZ")
		b := make([]rune, 3)
		for i := range b {
			b[i] = runes[r.Intn(len(runes))]
		}
		prefix := string(b)
		log.Printf("bidilisten: prefix %s\n", prefix)

		for i := 0; i < 100; i++ {
			send <- []byte("bidilisten: " + prefix + strconv.Itoa(i))
			time.Sleep(sleepDuration)
		}
	}()
	wg.Wait()
}
