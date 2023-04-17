// Application to debug UDP applications by writing to a port.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"log"
	"os"
	"strconv"
	"strings"
	"time"

	rnnet "remnav.com/remnav/net"
)

func counter(n int, sleep time.Duration, msgs chan<- []byte) {
	for i := 0; i < n; i++ {
		msgs <- []byte(strconv.Itoa(i))
		time.Sleep(sleep)
	}
}

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

	recvChans := rnnet.BidiWR(sendMsgs, devices, *dest, *bufSize, *verbose)
	go func() {
		for {
			for i, ch := range recvChans {
				select {
				case msg, ok := <-ch:
					if !ok {
						recvChans[i] = nil
						continue
					}
					fmt.Printf("bididial (recvChans[%d]) %s\n", i, string(msg))
				}
			}
		}
	}()

	if len(flag.Args()) == 0 {
		counter(*packets, sleepDuration, sendMsgs)
	} else {
		files(flag.Args(), sleepDuration, sendMsgs)
	}
	fmt.Print("here #5")
}
