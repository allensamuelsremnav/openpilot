// Listen for gpsd messages from gpsdrt.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"path/filepath"
	"strconv"
	"strings"

	gpsd "remnav.com/remnav/metadata/gpsd"
	rnnet "remnav.com/remnav/net"
)

// Make a Key from a presumed gpsd message.
func getter(msg []byte) rnnet.Key[int64] {
	var probe gpsd.Class
	err := json.Unmarshal([]byte(msg), &probe)
	if err != nil {
		log.Fatal(err)
	}
	return rnnet.Key[int64]{
		Type:      probe.Class,
		Timestamp: probe.Time.UnixMilli()}
}

// Send deduped gpsd messages from packet connection to channel.
func dedup(pc net.PacketConn, bufSize int) <-chan []byte {
	// Stuff packets into a chan []byte
	msgs := make(chan []byte)
	go func() {
		for {
			buf := make([]byte, bufSize)
			n, _, err := pc.ReadFrom(buf)
			if err != nil {
				log.Fatal(err)
			}
			msgs <- buf[:n]
		}
	}()
	return rnnet.Latest(msgs, getter)
}

// Forward messages to this UDP port.
func forward(msgs chan []byte, port int) {
	conn, err := net.Dial("udp", ":"+strconv.Itoa(port))
	if err != nil {
		log.Fatal(err)
	}
	for msg := range msgs {
		_, err := conn.Write(msg)
		if err != nil {
			log.Printf("port %d: %v\n", port, err)
		}
	}
}

func main() {
	listenHelp := fmt.Sprintf("listen on this port for UDP, e.g. %d",
		rnnet.OperatorGpsdListener)
	listenPort := flag.Int("listen", rnnet.OperatorGpsdListener, listenHelp)
	forwardHelp := fmt.Sprintf(
		"forward gpsd messages to this comma-separated list of ports, e.g. '%d,%d'",
		rnnet.OperatorGpsdDisplay,
		rnnet.OperatorGpsdTrajectory)

	forwardPorts := flag.String("forward", "", forwardHelp)
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	verbose := flag.Bool("verbose", false, "verbosity on")
	flag.Parse()

	progName := filepath.Base(os.Args[0])
	log.Printf("%s: listen port %d\n", progName, *listenPort)

	// Set up forwarding to the destination ports
	var forwards []int
	tokens := strings.Split(*forwardPorts, ",")
	for _, t := range tokens {
		if len(t) == 0 {
			continue
		}
		i, err := strconv.Atoi(strings.TrimSpace(t))
		if err != nil {
			log.Fatalf("unexpected forwarding ports '%s'\n", *forwardPorts)
		}
		forwards = append(forwards, i)
	}
	log.Printf("%s: forwarding to ports %v\n", progName, forwards)

	var forwardChannels []chan []byte
	for _, port := range forwards {
		ch := make(chan []byte)
		go forward(ch, port)
		forwardChannels = append(forwardChannels, ch)
	}

	// listen to incoming udp packets
	pc, err := net.ListenPacket("udp", ":"+strconv.Itoa(*listenPort))
	if err != nil {
		log.Fatal(err)
	}
	defer pc.Close()

	for msg := range dedup(pc, *bufSize) {
		for j := 0; j < len(forwardChannels); j++ {
			select {
			case forwardChannels[j] <- msg:
			default:
				log.Printf("%d channel not ready, packet dropped", forwards[j])
			}
		}
		if *verbose {
			fmt.Printf("gpsdlisten: %s", string(msg))
		}

	}
}
