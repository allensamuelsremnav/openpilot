// Mock trajectory planner.
package main

import (
	"encoding/json"
	"flag"
	"io/ioutil"
	"log"
	"math"
	"math/rand"
	"net"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"remnav.com/remnav/g920"
	rnlog "remnav.com/remnav/log"
	"remnav.com/remnav/metadata/gpsd"
	"remnav.com/remnav/metadata/storage"
	rnnet "remnav.com/remnav/net"
	trj "remnav.com/remnav/trajectory"
)

// Drain a channel (to keep it from blocking).
func sink(ch <-chan []byte) {
	go func() {
		for _ = range ch {
		}
	}()
}

// Make channels for the packets from a local UDP source.
func read(localPort, bufSize int) <-chan []byte {
	msgs := make(chan []byte)
	addr, err := net.ResolveUDPAddr("udp", ":"+strconv.Itoa(localPort))
	if err != nil {
		log.Fatal(err)
	}
	pc, err := net.ListenUDP("udp", addr)
	if err != nil {
		log.Fatal(err)
	}

	go func() {
		buf := make([]byte, bufSize)
		for {
			n, err := pc.Read(buf)
			if err != nil {
				log.Print(err)
				break
			}
			msgs <- buf[:n]
		}
		close(msgs)
	}()
	return msgs
}

// Drive steering wheel between += maxAngle with angSpeed sin wave (radians/s)
func g920Mock(intervalMs int, maxAngle, angSpeed float64) <-chan g920.G920 {
	ch := make(chan g920.G920)

	tStart := time.Now().UnixMicro()
	interval := time.Duration(intervalMs) * time.Millisecond
	go func() {
		defer close(ch)
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			tNow := <-ticker.C
			sinAngle := angSpeed * (float64)(tNow.UnixMicro()-tStart) / 1e6

			var g g920.G920
			g.Class = g920.ClassG920
			g.Requested = time.Now().UnixMicro()
			// Steering wheel deflection in radians.
			g.Wheel = maxAngle * math.Sin(sinAngle)
			ch <- g
		}
	}()

	return ch
}

func g920Device(vid, pid uint64, verbose, progress bool) <-chan g920.G920 {
	dev := g920.Open(vid, pid, verbose)
	return g920.Read(dev, progress)

}

// Generate TPV messages with speed.
func tpvMock() <-chan []byte {
	ch := make(chan []byte)

	const speed = 4.69
	const interval = time.Second
	go func() {
		defer close(ch)
		ticker := time.NewTicker(interval)
		defer ticker.Stop()
		for {
			tNow := <-ticker.C
			var tpv gpsd.TPV
			tpv.Class = gpsd.ClassTPV
			tpv.Time = tNow
			tpv.Mode = gpsd.Mode2D
			tpv.Speed = speed + rand.NormFloat64() * 0.01
			bytes, err := json.Marshal(tpv)
			if err != nil {
				log.Fatal(err)
			}
			ch <- bytes
		}
	}()

	return ch
}

// Read tpv messages from gpsdlistener.
func tpvPort(localPort, bufSize int) <-chan []byte {
	return read(localPort, bufSize)
}

// Use VehicleMock to convert trajectories into applications.
func vehicleListenMock(trajCh <-chan []byte) <-chan []byte {
	vehicleMock := trj.VehicleMock{RTT: 115, ApplicationDelay: 155, ExecutionPeriod: 50}
	return vehicleMock.Run(trajCh)
}

// Clone a channel.
func clone(ch <-chan []byte) (<-chan []byte, <-chan []byte) {
	out0 := make(chan []byte)
	out1 := make(chan []byte)
	go func() {
		defer close(out0)
		defer close(out1)
		for t := range ch {
			out0 <- t
			out1 <- t
		}
	}()
	return out0, out1
}

// Merge two channels into one; log the result.
func merge(trajCh, applCh <-chan []byte) (<-chan []byte, <-chan []byte) {
	out := make(chan []byte)
	log := make(chan []byte)
	var wg sync.WaitGroup
	output := func(msgs <-chan []byte) {
		for msg := range msgs {
			out <- msg
			log <- msg
		}
		wg.Done()
	}
	wg.Add(1)
	go output(trajCh)
	wg.Add(1)
	go output(applCh)
	go func() {
		wg.Wait()
		close(out)
		close(log)
	}()
	return out, log
}

func main() {
	config := flag.String("config", "", "configuration file")
	display := flag.Int("display",
		rnnet.OperatorOverlayListen,
		"trajectories and trajectory-appplication to this local port")

	logRoot := flag.String("log_root", "D:/remnav_log", "root for log storage")

	mock := flag.Bool("mock", false, "use mocks only; ignore switches for tpv and vidpid ports and g920 vidpid.")
	tpvPort_ := flag.Int("tpv_port",
		rnnet.OperatorGpsdTrajectory,
		"receive TPV messages from this local port (gpsdlisten). use 0 for mock.")
	defaultVidPID := "046d:c262"
	vidPID := flag.String("vidpid", defaultVidPID, "colon-separated g920 hex vid and pid. use 0:0 for mock.")
	vehiclePort := flag.Int("vehicle_port",
		rnnet.OperatorTrajectoryListen,
		"send trajectory requests and received applied messages using bidi. use 0 for mock.")

	bufSize := flag.Int("bufsize",
		4096,
		"buffer size for network packets")

	progress := flag.Bool("progress", false, "show progress indicator")
	verbose := flag.Bool("verbose", false, "verbosity on")

	flag.Parse()

	// g920 ids
	ids := strings.Split(*vidPID, ":")
	if len(ids) != 2 {
		log.Fatalf("invalid -vidpid argument %s, expected X:Y", *vidPID)
	}
	vid, err := strconv.ParseUint(ids[0], 16, 16)
	if err != nil {
		log.Fatal(err)
	}
	pid, err := strconv.ParseUint(ids[1], 16, 16)
	if err != nil {
		log.Fatal(err)
	}


	// Planner configuration
	var configBuf []byte
	if *config != "" {
		var err error
		configBuf, err = ioutil.ReadFile(*config)
		if err != nil {
			log.Fatal(err)
		}
		log.Printf("%s: parameter configuration %s\n", os.Args[0], *config)
	}
	params := trj.Parameters(configBuf)
	log.Printf("%s: params: %v\n", os.Args[0], params)

	// Set up live or mocks for speed, gpsd, vehicle bidi
	var speeds <-chan []byte
	if *tpvPort_ == 0 || *mock {
		speeds = tpvMock()
	} else {
		speeds = tpvPort(*tpvPort_, *bufSize)
	}

	var gs <-chan g920.G920
	if vid == 0  || *mock {
		const maxSteeringWheel = math.Pi / 8
		const sinSpeed = 2 * math.Pi / 4.0
		const reportIntervalMs = 5
		gs = g920Mock(reportIntervalMs, maxSteeringWheel, sinSpeed)
	} else {
		log.Printf("%s: vid:pid %04x:%04x (%d:%d)\n", os.Args[0], vid, pid, vid, pid)
		gs = g920Device(vid, pid, *verbose, *progress)
	}

	var vehicleListen func(<-chan []byte) <-chan []byte
	if *vehiclePort == 0  || *mock {
		vehicleListen = vehicleListenMock
	} else {
		vehicleListen = func(trajCh <-chan []byte) <-chan []byte {
			recvd := rnnet.BidiRW(*vehiclePort, *bufSize, trajCh, *verbose)
			return trj.Dedup(recvd, *progress, *verbose)
		}
	}

	// Wire up the planner channels.
	trajCh, vehicleCh, _ := trj.Planner(params, speeds, gs, false)
	// Our vehicle doesn't log, so remember what we sent it.
	vehicleCh, vehicleLogCh := clone(vehicleCh)
	applCh := vehicleListen(vehicleCh)

	displayCh, displayLogCh := merge(trajCh, applCh)

	var wg sync.WaitGroup

	wg.Add(1)
	cmdsLogDir := gpsd.LogDir("plnr", *logRoot, storage.VehicleCmdSubdir, "", "")
	go rnlog.StringBinned(vehicleLogCh, cmdsLogDir, &wg)

	log.Printf("%s: display port %d", os.Args[0], *display)
	if *display >= 0 {
		wg.Add(1)
		go rnnet.WritePort(displayCh, *display, &wg)
	} else {
		sink(displayCh)
	}

	wg.Add(1)
	displayLogDir := gpsd.LogDir("plnr", *logRoot, storage.TrajectorySubdir, "", "")
	go rnlog.StringBinned(displayLogCh, displayLogDir, &wg)

	wg.Wait()
}
