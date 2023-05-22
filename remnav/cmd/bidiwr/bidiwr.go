// Make the dialer end of a bidi channel look like a local port.
package main

import (
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"

	rnnet "remnav.com/remnav/net"
)

// Write BidiWR replies to pc.
func writeTo(pc net.PacketConn, addrs <-chan net.Addr, replies <-chan []byte, wG *sync.WaitGroup,
	verbose bool) {
	defer wG.Done()
	// Most recently seen address.
	var addr *net.Addr
	for {
		select {
		case a, ok := <-addrs:
			if !ok {
				addrs = nil
				break
			}
			addr = &a
		case reply, ok := <-replies:
			if !ok {
				replies = nil
				return
			}
			if addr == nil {
				break
			}
			_, err := pc.WriteTo(reply, *addr)
			if err != nil {
				// Get some operating experience, then possibly just continue
				log.Fatal(err)
			}
			if verbose {
				fmt.Println(string(reply))
			}
		}
	}
}

func main() {
	localUsage := fmt.Sprintf("listen and reply and this local port, i.e. %d  or %d (trajectories or g920).",
		rnnet.VehicleTrajectoryRequestApplication, rnnet.VehicleG920)
	localPort := flag.Int("port", 0, localUsage)

	destUsage := fmt.Sprintf("destination address, i.e. 10.0.0.60:%d or %d  (trajectories or g920)",
		rnnet.OperatorTrajectoryListen, rnnet.OperatorG920Listen)
	dest := flag.String("dest", "", destUsage)
	bufSize := flag.Int("bufsize", 4096, "buffer size for reading")
	recvKey := flag.String("recv_key", "requested", "filter latest received message using this field")
	verbose := flag.Bool("verbose", false, "verbosity on")
	devs := flag.String("devices", "eth0,eth0", "comma-separated list of network devices")
	flag.Parse()

	if *localPort == 0 {
		log.Fatal("--port required")
	}
	if len(*dest) == 0 {
		log.Fatal("--dest required")
	}

	devices := strings.Split(*devs, ",")
	log.Printf("%s, local port %d", os.Args[0], *localPort)
	log.Printf("%s, destination %s", os.Args[0], *dest)
	log.Printf("%s, devices %v", os.Args[0], devices)

	pc, err := net.ListenPacket("udp", "127.0.0.1:"+strconv.Itoa(*localPort))
	if err != nil {
		log.Fatal(err)
	}
	defer pc.Close()

	// Turn reads from local port into channels to use as dialer -> listener channel
	// and addresses for reply on local port.
	pcReads, addrs := rnnet.ReadFrom(pc, 4096)

	var wg sync.WaitGroup

	replies := rnnet.BidiWR(pcReads, devices, *dest, *bufSize, &wg, *verbose)
	filtered := rnnet.LatestInt64(replies, *recvKey)
	go writeTo(pc, addrs, filtered, &wg, *verbose)
	wg.Wait()

}
