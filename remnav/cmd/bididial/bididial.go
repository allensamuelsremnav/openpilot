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
func counter(n int, sleep time.Duration, msgs chan<- []byte) {
	r := rand.New(rand.NewSource(time.Now().UnixNano()))

	var runes = []rune("abcdefghijklmnopqrstuvwxyz")
	b := make([]rune, 3)
	for i := range b {
		b[i] = runes[r.Intn(len(runes))]
	}
	prefix := string(b)
	log.Printf("bididial: prefix %s\n", prefix)

	for i := 0; i < n; i++ {
		msgs <- []byte(prefix + "_" + strconv.Itoa(i))
		time.Sleep(sleep)
	}
}

// Send file contents.
func files(filenames []string, sleep time.Duration, msgs chan<- []byte) {
	for _, fn := range filenames {
		f, err := os.Open(fn)
		if err != nil {
			log.Fatal(err)
		}
		defer f.Close()
		s := bufio.NewScanner(f)
		for s.Scan() {
			msgs <- []byte(s.Text())
			time.Sleep(sleep)
		}
		if err := s.Err(); err != nil {
			log.Fatal(err)
		}
	}

}

func main() {
	dest := flag.String("dest", "10.0.0.11:6001", "destination address")
	bufSize := flag.Int("bufsize", 4096, "buffer size for reading")
	verbose := flag.Bool("verbose", false, "verbosity on")
	packets := flag.Int("packets", 100, "number of test packets")
	devs := flag.String("devices", "eth0,wlan0", "comma-separated list of network devices, e.g. eth0,wlan0")
	sleep := flag.Int("sleep", 100, "sleep between packets, microseconds")
	flag.Parse()
	devices := strings.Split(*devs, ",")
	sleepDuration := time.Duration(*sleep) * time.Microsecond
	log.Printf("%s, destination %s", os.Args[0], *dest)
	log.Printf("%s, devices %v", os.Args[0], devices)
	log.Printf("%s, sleep %v ", os.Args[0], sleepDuration)

	sendMsgs := make(chan []byte)
	defer close(sendMsgs)

	// Read the messages received on the back channel.
	var recvChan <-chan []byte = rnnet.BidiWR(sendMsgs, devices, *dest, *bufSize, *verbose)
	var recvWG sync.WaitGroup
	recvWG.Add(1)
	go func() {
		defer recvWG.Done()
		for msg := range recvChan {
			fmt.Printf("bididial: (recvchan) '%s'\n", string(msg))
		}
	}()

	// Send dialer-->listener messages.
	if len(flag.Args()) == 0 {
		counter(*packets, sleepDuration, sendMsgs)
	} else {
		files(flag.Args(), sleepDuration, sendMsgs)
	}

	recvWG.Wait()
}
