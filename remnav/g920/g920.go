package g920

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"math"
	"time"
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
	Class       string  `json:"class"`
	Requested   int64   `json:"requested"` // μs since Unix epoch
	Wheel       float64 `json:"wheel"`
	PedalMiddle float64 `json:"pedalmiddle"`
	PedalRight  float64 `json:"pedalright"`
}

func AsG920(buf []byte) G920 {
	var m G920
	m.Class = ClassG920
	m.Requested = time.Now().UnixMicro()
	wheel := 256*float64(buf[WheelHighByte]) + float64(buf[WheelLowByte])
	m.Wheel = (wheel - 256*128) * 2 * math.Pi * 5 / 4
	m.PedalMiddle = 1 - float64(buf[PedalMiddle])/256
	m.PedalRight = 1 - float64(buf[PedalRight])/256
	return m
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
	if probe.Class != ClassG920 {
		return 0, errors.New(fmt.Sprintf("unexpected class %s", probe.Class))
	}
	return probe.Requested, nil
}
