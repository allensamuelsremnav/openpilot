package g920

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
)

const ReportLength = 10

const ReportType = 0
const StateReport = 1

const DpadXboxABXY = 1
const ButtonsFlappy = 2
const Unknown3 = 3
const WheelLowByte = 4
const WheelHighByte = 5

// Pedals 255 --> 0
const PedalRight = 6
const PedalMiddle = 7
const PedalLeft = 8
const Unknown9 = 9

type Report struct {
	DpadXboxABXY  int
	ButtonsFlappy int
	Wheel         int
	PedalLeft     int
	PedalMiddle   int
	PedalRight    int
	Unknown3      int
	Unknown9      int
}

func Decode(buf []byte) (Report, error) {

	if len(buf) != ReportLength {
		log.Printf("unexpected report length %d\n", len(buf))
	}

	if buf[ReportType] != StateReport {
		log.Fatalf("unknown report type %d", buf[ReportType])
	}
	if buf[Unknown3] != 0 {
		log.Fatalf("unexpected byte 3 %v", buf)
	}
	if buf[Unknown9] != 5 {
		log.Fatalf("unexpected byte 9 %v", buf)
	}

	var report Report
	report.Wheel = 256*int(buf[WheelHighByte]) + int(buf[WheelLowByte])
	report.PedalLeft = int(buf[PedalLeft])
	report.PedalMiddle = int(buf[PedalMiddle])
	report.PedalRight = int(buf[PedalRight])

	report.DpadXboxABXY = int(buf[DpadXboxABXY])
	report.ButtonsFlappy = int(buf[ButtonsFlappy])
	report.Unknown3 = int(buf[Unknown3])
	report.Unknown9 = int(buf[Unknown9])
	return report, nil
}

const ClassG920 = "G920"

type G920 struct {
	Class     string `json:"class"`
	Requested int64  `json:"requested"` // μs since Unix epoch
	Report    string `json:"report"`    // base64
}

type tsProbe struct {
	Class     string `json:"class"`
	Requested int64  `json:"requested"`
}

// Extract the timestamp.
func Timestamp(msg []byte) (int64, error) {
	var probe tsProbe
	err := json.Unmarshal(msg, &probe)
	if err != nil {
		return 0, err
	}
	if probe.Class == ClassG920 {
		return probe.Requested, nil
	}
	return 0, errors.New(fmt.Sprintf("unexpected class %s", probe.Class))
}

const ClassHeartbeat = "HEARTBEAT"

type Heartbeat struct {
	Class string `json:"class"`
}
