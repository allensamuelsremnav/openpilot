package gpsd

import (
	"log"
	"time"
)

// Track is tempting, but can't be done locally because heading wraps at North.
type Position struct {
	Time  time.Time
	Lat   float64
	Lon   float64
	Speed float64
}

func Interpolate(tpvs []TPV, probe time.Time, start int) (*Position, int) {
	// Returns an interpolated position at probe time and the
	// index of the containing interval. Returns nil if
	// interpolation is not possible and a negative integer.

	// tpvs must be in time order.
	// Begin the linear search at start; this is an optimization for
	// the expected use case. 0 is always safe.
	if start < 0 {
		return nil, -1
	}
	for i := start; i < len(tpvs)-1; i++ {
		tpv := tpvs[i]
		if probe.Before(tpv.Time) {
			continue
		}

		next := tpvs[i+1]
		if next.Time.Before(tpv.Time) {
			log.Fatalf("TPVS at %d and %d are not in temporal order (%v vs %v)",
				i, i+1, tpv.Time, next.Time)
		}
		if next.Time.Before(probe) {
			// Interval at i does not contain probe.
			continue
		}
		// Use interval at i to interpolate.
		dt := float64(probe.Sub(tpv.Time)) / float64(next.Time.Sub(tpv.Time))
		lat := tpv.Lat
		lon := tpv.Lon
		speed := tpv.Speed
		return &Position{
			Time:  probe,
			Lat:   lat + dt*(next.Lat-lat),
			Lon:   lon + dt*(next.Lon-lon),
			Speed: speed + dt*(next.Speed-speed)}, i
	}
	return nil, -1
}
