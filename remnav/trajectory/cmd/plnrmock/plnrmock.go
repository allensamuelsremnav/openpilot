// Mock trajectory planner.
package main

import (
	"encoding/json"
	"flag"
	"log"
	"math"
	"sync"
	"time"

	"remnav.com/remnav/g920"
	rnlog "remnav.com/remnav/log"
	"remnav.com/remnav/metadata/gpsd"
	"remnav.com/remnav/metadata/storage"
	rnnet "remnav.com/remnav/net"
	trj "remnav.com/remnav/trajectory"
)

func sink(ch <-chan []byte) {
	go func() {
		for _ = range ch {
		}
	}()
}

// Drive steering wheel between += maxAngle with angSpeed sin wave (radians/s)
func g920s(intervalMs int, maxAngle, angSpeed float64) <-chan g920.G920 {
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

// Generate TPV messages with speed.
func tpvs() <-chan []byte {
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
			tpv.Speed = speed
			bytes, err := json.Marshal(tpv)
			if err != nil {
				log.Fatal(err)
			}
			ch <- bytes
		}
	}()

	return ch
}

// Use VehicleMock to convert trajectories into applications.
func applications(trajCh <-chan []byte) <-chan []byte {
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
	display := flag.Int("display",
		rnnet.OperatorOverlayListen,
		"trajectories and trajectory-appplication to this local port")

	logRoot := flag.String("log_root", "D:/remnav_log", "root for log storage")

	flag.Parse()
	params := trj.PlannerParameters{Wheelbase: 4,
		GameWheelToTire: 1.0, // Game wheel maps directly to tire angle.
		TireMax:         math.Pi / 4,
		DtireDtMax:      0.25, // 0.25 is sluggish
		Interval:        50,
	}

	speeds := tpvs()
	const maxSteeringWheel = math.Pi / 8
	const sinSpeed = 2 * math.Pi / 4.0
	gs := g920s(5, maxSteeringWheel, sinSpeed)

	trajCh, vehicleCh, _ := trj.Planner(params, speeds, gs, false)
	// Our vehicle mock doesn't log, so remember what we sent it.
	vehicleMockCh, vehicleLogCh := clone(vehicleCh)
	applCh := applications(vehicleMockCh)

	displayCh, displayLogCh := merge(trajCh, applCh)

	var wg sync.WaitGroup

	wg.Add(1)
	cmdsLogDir := gpsd.LogDir("plnrmock", *logRoot, storage.VehicleCmdSubdir, "", "")
	go rnlog.StringBinned(vehicleLogCh, cmdsLogDir, &wg)

	log.Printf("display port :%d", *display)
	if *display >= 0 {
		wg.Add(1)
		go rnnet.WritePort(displayCh, *display, &wg)
	} else {
		sink(displayCh)
	}

	wg.Add(1)
	displayLogDir := gpsd.LogDir("plnrmock", *logRoot, storage.TrajectorySubdir, "", "")
	go rnlog.StringBinned(displayLogCh, displayLogDir, &wg)

	wg.Wait()
}
