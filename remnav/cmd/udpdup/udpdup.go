// Test program for sending duplicate data over modems.
package main

import (
	"bufio"
	"flag"
	"log"
	"os"
	"strconv"
	"strings"
	"sync"
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
	dest := flag.String("dest",
		"10.0.0.60:"+strconv.Itoa(rnnet.OperatorGpsdListen),
		"destination host:port, e.g. 96.64.247.70:"+strconv.Itoa(rnnet.OperatorGpsdListen))
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

	msgs := make(chan []byte)
	defer close(msgs)

	var wg sync.WaitGroup
	rnnet.UDPDup(msgs, devices, *dest, &wg, *verbose)

	if len(flag.Args()) == 0 {
		counter(*packets, sleepDuration, msgs)
	} else {
		files(flag.Args(), sleepDuration, msgs)
	}
	wg.Wait()
}
