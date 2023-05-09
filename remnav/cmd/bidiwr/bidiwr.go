// Make the dialer end of a bidi channel look like a local port.  Does not dedup.
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
	localPort := flag.Int("port", rnnet.VehicleTrajectoryRequestApplication, "listen and reply on this local port")
	destDefault := fmt.Sprintf("10.0.0.60:%d", rnnet.OperatorTrajectoryListen)
	dest := flag.String("dest", destDefault, "destination address")
	bufSize := flag.Int("bufsize", 4096, "buffer size for reading")
	verbose := flag.Bool("verbose", false, "verbosity on")
	devs := flag.String("devices", "eth0,eth0", "comma-separated list of network devices")
	flag.Parse()

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
	go writeTo(pc, addrs, replies, &wg, *verbose)
	wg.Wait()

}
