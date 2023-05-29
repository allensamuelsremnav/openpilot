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
	go func() {
		var tpv gpsd.TPV
		tpv.Class = "TPV"
		tpv.Time = time.Now()
		tpv.Speed = speed
		bytes, err := json.Marshal(tpv)
		if err != nil {
			log.Fatal(err)
		}
		ch <- bytes
	}()

	return ch

}
func main() {
	display := flag.Int("display",
		rnnet.OperatorOverlayListen,
		"trajectories and trajectory-appplication to this local port")

	logRoot := flag.String("log_root", "D:/remnav_log", "root for log storage")

	flag.Parse()
	params := trj.Parameters{Wheelbase: 4,
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

	var wg sync.WaitGroup

	wg.Add(1)
	go rnlog.Binned(logCh, logDir, &wg)

	wg.Add(1)
	go rnnet.WritePort(trajCh, *display, &wg)

	wg.Wait()
}
