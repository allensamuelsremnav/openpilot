// Mock operator trajectory planner and trajectory listener.
package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"

	rnlog "remnav.com/remnav/log"
	"remnav.com/remnav/metadata/gpsd"
	"remnav.com/remnav/metadata/storage"
	rnnet "remnav.com/remnav/net"
	trj "remnav.com/remnav/trajectory"
)

func main() {
	listen := flag.Int("listen",
		rnnet.OperatorTrajectoryListen,
		"port for trajectory requests and applied messages")

	localApplied := flag.Int("local_applied",
		rnnet.OperatorTrajectoryApplication,
		"forward trajectory-appplication to this local port")
	// Since we are mocking trajectory generation, we have to send the mocks to the display.
	localTraj := flag.Int("local_trajectory",
		rnnet.OperatorTrajectoryRequestDisplay,
		"forward trajectory request to this local port")

	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")
	sleep := flag.Int("sleep", 50, "sleep between trajectories, ms")

	// Parameters for the mock
	curvature := flag.Float64("curvature", 1.0/50.0, "const radius of curvature, 1/m")
	speed := flag.Float64("speed", 5, "const trajectory speed in m/s")

	progress := flag.Bool("progress", false, "show progress indicator")
	logRoot := flag.String("log_root", "D:/remnav_log", "root for log storage")
	verbose := flag.Bool("verbose", false, "verbosity on")
	flag.Parse()

	progName := filepath.Base(os.Args[0])

	sleepDuration := time.Duration(*sleep) * time.Millisecond

	logDir := gpsd.LogDir("trajectory", *logRoot, storage.TrajectorySubdir, "", "")
	logCh := make(chan []byte, 2) // Need a small rate buffer at 100 Hz.

	send := make(chan []byte)
	recvd := rnnet.BidiRW(*listen, *bufSize, send, *verbose)
	deduped := trj.Dedup(recvd, *progress, *verbose)

	var wg sync.WaitGroup
	wg.Add(1)
	go rnlog.Binned(logCh, trj.Timestamp, logDir, &wg)

	// Application messages go to the display.
	wg.Add(1)
	applCh := make(chan []byte)
	go rnnet.WritePort(applCh, *localApplied, &wg)

	go func() {
		for msg := range deduped {
			applCh <- msg

			select {
			case logCh <- msg:
			default:
				log.Printf("%s: log channel not ready (annotation)\n", progName)
			}
		}
	}()

	// Trajectory requests go to the display as well as to the vehicle.
	wg.Add(1)
	localTrajCh := make(chan []byte)
	go rnnet.WritePort(localTrajCh, *localTraj, &wg)

	wg.Add(1)
	go func() {
		defer wg.Done()
		for {
			t := time.Now().UnixMicro()
			trajectory, _ := json.Marshal(
				map[string]interface{}{
					"class":     trj.ClassTrajectory,
					"requested": t,
					"curvature": *curvature,
					"speed":     *speed,
				})
			send <- trajectory
			localTrajCh <- trajectory
			if *progress {
				fmt.Printf("t")
			}
			if *verbose {
				fmt.Println(string(trajectory))
			}
			select {
			case logCh <- trajectory:
			default:
				log.Printf("%s: log channel not ready (trajectory)\n", progName)
			}
			time.Sleep(sleepDuration)
		}
		close(send)
	}()
	wg.Wait()
	close(logCh)
}
