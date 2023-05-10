// Mock operator trajectory planner.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"sync"
	"time"

	rnlog "remnav.com/remnav/log"
	"remnav.com/remnav/metadata/gpsd"
	"remnav.com/remnav/metadata/storage"
	rnnet "remnav.com/remnav/net"
	traj "remnav.com/remnav/trajectory"
)

func main() {
	listen := flag.Int("listen", rnnet.OperatorTrajectoryListen, "port for trajectory requests and applied messages")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	sleep := flag.Int("sleep", 50, "sleep between trajectories, ms")
	curvature := flag.Float64("curvature", 1.0/50.0, "const radius of curvature, 1/m")
	speed := flag.Float64("speed", 5, "const trajectory speed in m/s")
	progress := flag.Bool("progress", false, "show progress indicator")
	logRoot := flag.String("log_root", "D:/remnav_log", "root for log storage")
	verbose := flag.Bool("verbose", false, "verbosity on")
	flag.Parse()

	sleepDuration := time.Duration(*sleep) * time.Millisecond

	logDir := gpsd.LogDir("trajectory", *logRoot, storage.TrajectorySubdir, "", "")
	logCh := make(chan []byte, 2) // Need a small rate buffer at 100 Hz.

	send := make(chan []byte)
	recvd := rnnet.BidiRW(*listen, *bufSize, send, *verbose)

	var wg sync.WaitGroup
	wg.Add(1)
	go rnlog.Binned(logCh, logDir, &wg)
	wg.Add(1)
	go func() {
		defer wg.Done()
		var latest int64
		for msg := range recvd {
			// Unmarshall, dedup, insert operator time, marshall, then log.
			var applied traj.TrajectoryApplication
			err := json.Unmarshal(msg, &applied)
			if err != nil {
				log.Fatal(err)
			}
			if applied.Trajectory > latest {
				if *progress {
					fmt.Printf("a")
				}
				latest = applied.Trajectory
				applied.Log = time.Now().UnixMicro()
				annotated, _ := json.Marshal(applied)
				if *verbose {
					fmt.Println(string(annotated))
				}
				select {
				case logCh <- annotated:
				default:
					log.Printf("%s: log channel not ready (annotation)\n", os.Args[0])
				}
			}
		}
	}()

	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			t := time.Now().UnixMicro()
			trajectory, _ := json.Marshal(
				map[string]interface{}{
					"class":     traj.ClassTrajectory,
					"requested": t,
					"curvature": *curvature,
					"speed":     *speed,
					"log":       t,
				})
			send <- trajectory
			if *verbose {
				fmt.Println(string(trajectory))
			}
			select {
			case logCh <- trajectory:
			default:
				log.Printf("%s: log channel not ready (trajectory)\n", os.Args[0])
			}
			time.Sleep(sleepDuration)
		}
		close(send)
	}()
	wg.Wait()
	close(logCh)
}
