// Sample program for vehicle trajectory communication.
package main

import (
	"encoding/json"
	"errors"
	"flag"
	"fmt"
	"log"
	"net"
	"strconv"
	"sync"
	"time"

	g920 "remnav.com/remnav/g920"
	rnlog "remnav.com/remnav/log"
	gpsd "remnav.com/remnav/metadata/gpsd"
	"remnav.com/remnav/metadata/storage"
	rnnet "remnav.com/remnav/net"
)

func main() {
	localPort := flag.Int("port", rnnet.VehicleG920, "listen and reply on this local port")
	heartbeat := flag.Int("heartbeat", 50, "interval between heartbeats, millieconds")

	bufSize := flag.Int("bufsize", 4096, "buffer size for reading")
	logRoot := flag.String("log_root",
		"/home/greg/remnav_log",
		"vehicle storage directory, e.g. '/home/user/6TB/vehicle/remconnect'")

	progress := flag.Bool("progress", false, "show progress indicator")
	flag.Parse()

	heartbeatDuration := time.Duration(*heartbeat) * time.Millisecond

	logDir := gpsd.LogDir("g920", *logRoot, storage.G920Subdir, "", "")
	logCh := make(chan []byte, 2)

	var wg sync.WaitGroup
	wg.Add(1)
	go rnlog.Binned(logCh, g920.Timestamp, logDir, &wg)

	// Local port to bidiwr.
	pc, err := net.Dial("udp", ":"+strconv.Itoa(*localPort))
	if err != nil {
		log.Fatal(err)
	}

	// Send heartbeats forever.
	var hb g920.Heartbeat
	hb.Class = g920.ClassHeartbeat
	beat, _ := json.Marshal(hb)
	go func() {
		for {
			_, err := pc.Write(beat)
			if err != nil {
				log.Fatal(err)
			}
			if *progress {
				fmt.Printf("h")
			}
			time.Sleep(heartbeatDuration)
		}
	}()

	// Use this code to measure report rate, which can be 450 reports/s or higher.
	reportCounter := make(chan bool)
	go func() {
		interval := time.Minute
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		var reportCount int
		for {
			select {
			case <-reportCounter:
				reportCount += 1
			case <-ticker.C:
				log.Printf("%.0f reports/s",
					float64(reportCount)/interval.Seconds())
				reportCount = 0
			}
		}
	}()

	for {
		buf := make([]byte, *bufSize)
		n, err := pc.Read(buf)
		if err != nil {
			log.Fatal(err)
		}

		logCh <- buf[:n]
		if *progress {
			fmt.Printf("g")
		}

		select {
		case reportCounter <- true:
		default:
		}

		// Do something with the report
		var report g920.G920
		err = json.Unmarshal(buf[:n], &report)
		if err != nil {
			log.Fatal(err)
		}
		if report.Class != g920.ClassG920 {
			log.Fatal(errors.New(fmt.Sprintf("expected class %s, got %s",
				g920.ClassG920, report.Class)))
		}

	}

}
