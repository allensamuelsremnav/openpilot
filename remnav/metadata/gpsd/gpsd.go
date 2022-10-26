// Package gpsd has structs for decoding messages from the gpds server.
package gpsd

import (
	"time"
)

type class struct {
	Class string
}

// gpsd/gps.h says that the units are degrees, meters, and seconds in
// the raw gpsd output.  Assume that the conversion to JSON preserves
// this.

type NMEAMode int

// NMEA GSA values
const (
	ModeUnknown NMEAMode = iota
	ModeNoFix
	Mode2D
	Mode3D
)

type TPV struct {
	Class  string
	Device string
	Mode   NMEAMode
	Time   time.Time
	Lat    float64
	Lon    float64
	Alt    float64

	EPX   float64
	EPY   float64
	EPV   float64
	SEP   float64 // 3d error
	Track float64
	// Remember degrees --> radians if we add track error.
	Speed float64
	EPS   float64 // speed error
}

type PRN struct {
	PRN  int64
	Used bool
}

type SKY struct {
	Class      string
	Device     string
	Time       time.Time
	XDOP       float64
	YDOP       float64
	VDOP       float64
	HDOP       float64
	PDOP       float64
	Satellites []PRN
}

type PPS struct {
	Class     string
	Device    string
	RealSec   int `json:"real_sec"`
	RealNsec  int `json:"real_nsec"`
	ClockSec  int `json:"clock_sec"`
	ClockNsec int `json:"clock_nsec"`
}

type Device struct {
	Path string
}
type Devices struct {
	Devices []Device
}
