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
			tpv.Class = "TPV"
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
func clone(trjCh <-chan []byte) (<-chan []byte, <-chan []byte) {
	out0 := make(chan []byte)
	out1 := make(chan []byte)
	go func() {
		defer close(out0)
		defer close(out1)
		for t := range trjCh {
			out0 <- t
			out1 <- t
		}
	}()
	return out0, out1
}

// Merge two channels into one.
func merge(ch0, ch1 <-chan []byte) <-chan []byte {
	out := make(chan []byte)
	go func() {
		defer close(out)
		ok0 := true
		ok1 := true
	forloop:
		for {
			var bytes []byte
			select {
			case bytes, ok0 = <-ch0:
				if !ok0 {
					ch0 = nil
					if !ok1 {
						break forloop
					}
				}
				out <- bytes
			case bytes, ok1 = <-ch1:
				if !ok1 {
					ch1 = nil
					if !ok0 {
						break forloop
					}
				}
				out <- bytes
			}
		}
	}()
	return out
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

	logDir := gpsd.LogDir("plnrmock", *logRoot, storage.TrajectorySubdir, "", "")
	logCh := make(chan rnlog.Loggable, 6)

	trajCh := trj.Planner(params, speeds, gs, logCh)
	trajCh0, trajCh1 := clone(trajCh)
	applCh := applications(trajCh0)
	// On Linux, at least, we can't open two connections to the same local port,
	// so we merge the applications and the trajectories.
	displayCh := merge(trajCh1, applCh)

	var wg sync.WaitGroup

	wg.Add(1)
	go rnlog.Binned(logCh, logDir, &wg)

	wg.Add(1)
	go rnnet.WritePort(displayCh, *display, &wg)

	wg.Wait()
}
