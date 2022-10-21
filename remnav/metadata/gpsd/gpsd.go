// Package gpsd has structs for decoding messages from the gpds server.
package gpsd

import (
	"encoding/json"
	"time"
)

type class struct {
	Class string
}

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
	Status json.Number
	Mode   NMEAMode
	Time   time.Time
	Lat    json.Number
	Lon    json.Number
	Alt    json.Number

	EPX   json.Number
	EPY   json.Number
	EPV   json.Number
	SEP   json.Number // 3d error
	Track json.Number
	Speed json.Number
	EPS   json.Number // speed error
}

type PRN struct {
	PRN  json.Number
	Used bool
}

type SKY struct {
	Class      string
	Device     string
	Time       time.Time
	XDOP       json.Number
	YDOP       json.Number
	VDOP       json.Number
	HDOP       json.Number
	PDOP       json.Number
	Satellites []PRN
}

type PPS struct {
	Class     string
	Device    string
	RealSec   json.Number `json:"real_sec"`
	RealNsec  json.Number `json:"real_nsec"`
	ClockSec  json.Number `json:"clock_sec"`
	ClockNsec json.Number `json:"clock_nsec"`
	Precision json.Number
}

type Devices struct {
	Devices []Device
}

type Device struct {
	Path string
}
