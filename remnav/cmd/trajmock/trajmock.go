// Mock operator trajectory planner.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"sync"
	"time"

	"remnav.com/remnav/net"
	rnnet "remnav.com/remnav/net"
	traj "remnav.com/remnav/trajectory"
)

func main() {
	listen := flag.Int("listen", net.OperatorTrajectoryListen, "port for trajectory requests and applied messages")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	sleep := flag.Int("sleep", 50, "sleep between trajectories, ms")
	curvature := flag.Float64("curvature", 1.0/50.0, "const radius of curvature, 1/m")
	speed := flag.Float64("speed", 5, "const trajectory speed in m/s")
	logRoot := flag.String("log_root", "D:/remnav_log", "root for log storage")
	verbose := flag.Bool("verbose", false, "verbosity on")
	flag.Parse()

	sleepDuration := time.Duration(*sleep) * time.Microsecond

	send := make(chan []byte)
	recvd := rnnet.BidiRW(*listen, *bufSize, send, *verbose)

	var wg sync.WaitGroup
	wg.Add(1)
	go func() {
		defer wg.Done()
		for msg := range recvd {
			// Unmarshall, insert operator time, marshall, then log.
			var applied traj.TrajectoryApplication
			err := json.Unmarshal(msg, &applied)
			if err != nil {
				log.Fatal(err)
			}
			applied.Operator = time.Now().UnixMicro()
			msg, _ := json.Marshal(applied)
			if *verbose {
				fmt.Println(string(msg))
			}
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			trajectory, _ := json.Marshal(
				map[string]interface{}{
					"class":     traj.ClassTrajectory,
					"time":      time.Now().UnixMicro(),
					"curvature": *curvature,
					"speed":     *speed,
				})
			send <- trajectory
			if *verbose {
				fmt.Println(string(trajectory))
			}
			time.Sleep(sleepDuration)
		}
	}()
	wg.Wait()
}
