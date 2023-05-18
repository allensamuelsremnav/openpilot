// Read g920 reports and forward to local port and to vehicle via BidiWR
package main

import (
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

func read(dev *hid.Device, bidiSend, localCh, logCh chan<- []byte, progName string, progress bool, wg *sync.WaitGroup) {
	pedalMiddle := -1.0
	pedalRight := -1.0
	for {
		buf := make([]byte, 65535)
		n, err := dev.Read(buf)
		if err != nil {
			log.Fatal(err)
		}

		m := g920.AsG920(buf[:n])
		msg, _ := json.Marshal(m)
		if pedalRight != m.PedalRight || pedalMiddle != m.PedalMiddle {
			bidiSend <- msg
			pedalMiddle, pedalRight = m.PedalMiddle, m.PedalRight
		}
		localCh <- msg
		select {
		case logCh <- msg:
		default:
			log.Printf("%s: log channel not ready\n", progName)
		}

		if progress {
			fmt.Printf("g")
		}
	}
}

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
	bidiSend := make(chan []byte)
	bidiRecvd := rnnet.BidiRW(*listen, *bufSize, bidiSend, *verbose)

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
	go rnnet.HeartbeatSink(bidiRecvd, time.Minute)

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

	wg.Add(1)
	go read(dev, bidiSend, localCh, logCh, progName, *progress, &wg)
	wg.Wait()
}
