// Read g920 reports and forward to local port and to vehicle via BidiWR
package main

import (
	"encoding/base64"
	"encoding/json"
	"flag"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"strconv"
	"strings"
	"sync"
	"time"

	"github.com/sstallion/go-hid"
	g920 "remnav.com/remnav/g920"
	rnlog "remnav.com/remnav/log"
	gpsd "remnav.com/remnav/metadata/gpsd"
	storage "remnav.com/remnav/metadata/storage"
	rnnet "remnav.com/remnav/net"
)

func main() {
	vidPID := flag.String("vidpid", "046d:c262", "colon-separated hex vid and pid")

	listen := flag.Int("listen",
		rnnet.OperatorG920Listen,
		"bidirectional port for G920 reports and heartbeats")
	bufSize := flag.Int("bufsize", 4096, "buffer size for incoming messages")

	localPort := flag.Int("local",
		rnnet.OperatorG920Trajectory,
		"send G920 reports to this local port for trajectory planner")

	logRoot := flag.String("log_root", "D:/remnav_log", "root for log storage")
	progress := flag.Bool("progress", false, "show progress indicator")
	verbose := flag.Bool("verbose", false, "verbosity on")

	flag.Parse()

	progName := filepath.Base(os.Args[0])

	// g920 ids
	ids := strings.Split(*vidPID, ":")
	vid, err := strconv.ParseUint(ids[0], 16, 16)
	if err != nil {
		log.Fatal(err)
	}
	pid, err := strconv.ParseUint(ids[1], 16, 16)
	if err != nil {
		log.Fatal(err)
	}

	log.Printf("%s: vid:pid %04x:%04x (%d:%d)\n", progName, vid, pid, vid, pid)
	log.Printf("%s: listen port %d\n", progName, *listen)
	log.Printf("%s: local port %v\n", progName, *localPort)

	// Set up as listener for bidirectional port.
	send := make(chan []byte)
	recvd := rnnet.BidiRW(*listen, *bufSize, send, *verbose)

	var wg sync.WaitGroup

	// Send to the local port.
	wg.Add(1)
	localCh := make(chan []byte)
	go rnnet.WritePort(localCh, *localPort, &wg)

	// Log
	wg.Add(1)
	logDir := gpsd.LogDir("g920", *logRoot, storage.G920Subdir, "", "")
	logCh := make(chan []byte, 2)
	go rnlog.Binned(logCh, g920.Timestamp, logDir, &wg)

	// Periodically report on bidi heartbeats.
	go func() {
		interval := time.Minute
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		var n int
		for {
			select {
			case <-recvd:
				n += 1
			case <-ticker.C:
				log.Printf("%.0f heartbeats/s",
					float64(n)/interval.Seconds())
				n = 0
			}
		}
	}()

	// Finally read the G920 reports.
	dev, err := hid.OpenFirst(uint16(vid), uint16(pid))
	if err != nil {
		log.Fatal(err)
	}

	info, err := dev.GetDeviceInfo()
	if err != nil {
		log.Fatal(err)
	}

	log.Printf("%s: ID %04x:%04x %s %s\n",
		info.Path,
		info.VendorID,
		info.ProductID,
		info.MfrStr,
		info.ProductStr)

	/*
		// Use this code to measure report rate, which can be 450 reports/s or higher.
		reportCounter := make(chan bool)
		go func() {
			interval := time.Second
			ticker := time.NewTicker(interval)
			defer ticker.Stop()
			var reportCount int
			for {
				select {
				case <-reportCounter:
					reportCount += 1
				case <-ticker.C:
					log.Printf("%.0f reports/s",
					float64(reportCount) / interval.Seconds())
					reportCount = 0
				}
			}
		}()
	*/

	for {
		buf := make([]byte, 65535)
		n, err := dev.Read(buf)
		if err != nil {
			log.Fatal(err)
		}

		var m g920.G920
		m.Class = g920.ClassG920
		m.Requested = time.Now().UnixMicro()
		m.Report = base64.StdEncoding.EncodeToString(buf[:n])
		msg, _ := json.Marshal(m)
		send <- msg
		localCh <- msg
		select {
		case logCh <- msg:
		default:
			log.Printf("%s: log channel not ready\n", progName)
		}

		/*
			select {
			case reportCounter <- true:
			default:
			}
		*/

		if *progress {
			fmt.Printf("g")
		}
		if *verbose {
			d, err := g920.Decode(buf[:n])
			if err != nil {
				log.Fatal(err)
			}
			fmt.Printf("wheel %6d, pedal (%3d, %3d, %3d), dpad_xboxabxy %3d, buttons_flappy %3d\n", d.Wheel-256*128, d.PedalLeft, d.PedalMiddle, d.PedalRight, d.DpadXboxABXY, d.ButtonsFlappy)
		}
	}

	wg.Wait()
}
