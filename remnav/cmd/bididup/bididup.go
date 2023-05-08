// Application to debug bidirectional dialer-->listener communication: dialer.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"math/rand"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	rnnet "remnav.com/remnav/net"
)

// Send numbered messages.
func counter(n int, sleep time.Duration) <-chan []byte {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))

	var runes = []rune("abcdefghijklmnopqrstuvwxyz")
	b := make([]rune, 3)
	for i := range b {
		b[i] = runes[r.Intn(len(runes))]
	}
	prefix := string(b)
	log.Printf("bididup: send prefix %s\n", prefix)

	msgs := make(chan []byte)

	go func() {
		for i := 0; i < n; i++ {
			msgs <- []byte(prefix + "_" + strconv.Itoa(i))
			time.Sleep(sleep)
		}
		close(msgs)
	}()
	return msgs
}

// Send file contents.
func files(filenames []string, sleep time.Duration) <-chan []byte {
	msgs := make(chan []byte)
	go func() {
		for _, fn := range filenames {
			f, err := os.Open(fn)
			if err != nil {
				log.Fatal(err)
			}
			s := bufio.NewScanner(f)
			for s.Scan() {
				msgs <- []byte(s.Text())
				time.Sleep(sleep)
			}
			if err := s.Err(); err != nil {
				log.Fatal(err)
			}
			f.Close()
		}
		close(msgs)
	}()
	return msgs
}

func main() {
	dest := flag.String("dest", "10.0.0.60:6001", "destination address")
	bufSize := flag.Int("bufsize", 4096, "buffer size for reading")
	verbose := flag.Bool("verbose", false, "verbosity on")
	packets := flag.Int("packets", 100, "number of test packets")
	devs := flag.String("devices", "eth0,wlan0", "comma-separated list of network devices, e.g. eth0,wlan0")
	sleep := flag.Int("sleep", 1000000, "sleep between packets, microseconds")
	flag.Parse()
	devices := strings.Split(*devs, ",")
	sleepDuration := time.Duration(*sleep) * time.Microsecond
	log.Printf("%s, destination %s", os.Args[0], *dest)
	log.Printf("%s, devices %v", os.Args[0], devices)
	log.Printf("%s, sleep %v ", os.Args[0], sleepDuration)

	// dialer-->listener messages.
	var sendMsgs <-chan []byte
	if len(flag.Args()) == 0 {
		sendMsgs = counter(*packets, sleepDuration)
	} else {
		sendMsgs = files(flag.Args(), sleepDuration)
	}

	// Wait on all sends and all receives.
	var wg sync.WaitGroup

	// dialer<--listener messages
	recvChan := rnnet.BidiWR(sendMsgs, devices, *dest, *bufSize,
		&wg, *verbose)

	wg.Add(1)
	go func() {
		defer wg.Done()
		for msg := range recvChan {
			fmt.Printf("bididup: (recvchan) '%s'\n", string(msg))
		}
	}()

	wg.Wait()
}
