// Sample program for vehicle trajectory communication.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"net"
	"strconv"
	"time"

	rnnet "remnav.com/remnav/net"
	traj "remnav.com/remnav/trajectory"
)

func main() {
	localPort := flag.Int("port", rnnet.VehicleTrajectoryRequestApplication, "listen and reply on this local port")
	bufSize := flag.Int("bufsize", 4096, "buffer size for reading")
	verbose := flag.Bool("verbose", false, "verbosity on")
	progress := flag.Bool("progress", false, "show progress indicator")
	timeout := flag.Int("timeout", 1000, "timeout for reads during initialization, millieconds")
	flag.Parse()

	timeoutDuration := time.Duration(*timeout) * time.Millisecond

	pc, err := net.Dial("udp", ":"+strconv.Itoa(*localPort))
	if err != nil {
		log.Fatal(err)
	}

	// The remote end needs to receive at least one packet to have
	// a reply-to address, so we just keep sending until we get
	// something back.
	buf := make([]byte, *bufSize)
	var n int // valid portion of buf.
	probe, _ := json.Marshal(
		map[string]interface{}{
			"class":      traj.ClassTrajectoryApplication,
			"trajectory": 0,
			"applied":    0,
			"log":        0,
		})

	for {
		_, err := pc.Write(probe)
		if err != nil {
			log.Fatal(err)
		}

		// Don't let the read block forever since the probes might be lost
		// or the other listener might not be ready.
		deadline := time.Now().Add(timeoutDuration)
		err = pc.SetReadDeadline(deadline)
		if err != nil {
			log.Fatal(err)
		}

		n, err = pc.Read(buf)
		if err != nil {
			log.Printf("waiting to establish bidi: %v", err)
			continue
		}
		if n > 0 {
			if *verbose {
				fmt.Println(string(buf[:n]))
			}
			break
		}
	}

	if n <= 0 {
		log.Fatalf("programming error, expected %d == 0", n)

	}

	// Handle requests and applied messages synchronously.  This is for illustration.
	// Any errors are fatal; this is also for illustration.

	// Clear the read deadline.
	err = pc.SetReadDeadline(time.Time{})
	if err != nil {
		log.Fatal(err)
	}

	for {
		var trajectory traj.Trajectory
		err := json.Unmarshal(buf[:n], &trajectory)
		if err != nil {
			log.Fatal(err)
		}

		// Handle trajectory request
		if *progress {
			fmt.Printf("t")
		}
		if *verbose {
			fmt.Println(string(buf[:n]))
		}

		// Send applied message
		applied, _ := json.Marshal(
			map[string]interface{}{
				"class":      traj.ClassTrajectoryApplication,
				"trajectory": trajectory.Requested,
				"applied":    time.Now().UnixMicro(),
				"log":        0,
			})

		_, err = pc.Write(applied)
		if err != nil {
			log.Fatal(err)
		}

		if *progress {
			fmt.Printf("a")
		}
		if *verbose {
			fmt.Println(string(applied))
		}

		// Wait for next request
		n, err = pc.Read(buf)
		if err != nil {
			log.Fatal(err)
		}

	}
}
