// Application to debug bidirectional dialer-->listener communication: listener.
package main

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
	sleep := flag.Int("sleep", 1000000, "sleep between packets, microseconds")
	packets := flag.Int("packets", 100, "number of test packets to send")
	echo := flag.Bool("echo", false, "echo on")
	verbose := flag.Bool("verbose", false, "verbosity on")
	flag.Parse()

	sleepDuration := time.Duration(*sleep) * time.Microsecond

	progName := filepath.Base(os.Args[0])
	log.Printf("%s: port %d\n", progName, *port)

	send := make(chan []byte)
	recvd := rnnet.BidiRW(*port, *bufSize, send, *verbose)

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for msg := range recvd {
			fmt.Printf("bidilisten: (recvd) %d bytes, '%s'\n", len(msg), string(msg))
			if *echo {
				send <- []byte("bidilisten: (echo) " + string(msg))
			}
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
		log.Printf("bidilisten: send prefix %s\n", prefix)

		for i := 0; i < *packets; i++ {
			sendMsg := []byte("bidilisten: (send) " + prefix + "_" + strconv.Itoa(i))
			send <- sendMsg
			time.Sleep(sleepDuration)
		}
		close(send)
	}()
	wg.Wait()
}
