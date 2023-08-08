package trajectory

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"math"
	"time"

	g920 "remnav.com/remnav/g920"
	gpsd "remnav.com/remnav/metadata/gpsd"
)

// Speed from gpsd.TPV message.
type Speed struct {
	Class string `json:"class"`
	// Received at planner
	Received int64 `json:"received"` // μs since Unix epoch
	// Gpsd epoch time.
	Gpsd  int64   `json:"gpsd"`   // μs since Unix epoch
	Speed float64 `json':"speed"` // m/s
}

func (s Speed) Timestamp() (int64, error) {
	return s.Received, nil
}

func (s Speed) bytes() []byte {
	bytes, err := json.Marshal(s)
	if err != nil {
		log.Fatal(err)
	}
	return bytes
}

func (s Speed) String() string {
	return string(s.bytes())
}

// Parameters for the planner
type PlannerParameters struct {
	Wheelbase float64 // m
	// Convert game wheel positions to tire angle.
	GameWheelToTire float64 // radians/radians
	// Maximum allowed tire angle
	TireMax float64 // radians
	// Tire steering max angular velocity
	DtireDtMax float64 // radians/s
	// 50 ms at 20 Hz.
	Interval int64 // ms
}

// Convert game wheel to tire angle.
func (p PlannerParameters) tire(gWheel float64) float64 {
	tire := gWheel * p.GameWheelToTire
	if math.Abs(tire) > p.TireMax {
		tire = math.Copysign(p.TireMax, tire)
	}
	return tire
}

// Limit tire angle rate of change (ω).
// radians and seconds!!!
func (p PlannerParameters) limitDtireDt(tireState, tireRequested, dt float64) float64 {
	if p.DtireDtMax <= 0 {
		return tireRequested
	}
	dtire := tireRequested - tireState
	if dt == 0 && dtire != 0 {
		log.Fatalf("dt == 0 but dtire = %v", dtire)
	}
	newTire := tireRequested
	if math.Abs(dtire) > dt*p.DtireDtMax {
		dtireLimited := math.Copysign(dt*p.DtireDtMax, dtire)
		newTire = tireState + dtireLimited
	}
	return newTire
}

// Compute curvature using front-axle bicycle model.
func (p PlannerParameters) curvature(tire float64) float64 {
	return math.Sin(tire) / p.Wheelbase
}


// Initialize PlannerParams, possibly from data.
func Parameters(buf []byte) PlannerParameters {
	var params PlannerParameters
	var defaultInterval int64 = 50
	params.Interval = defaultInterval

	if buf != nil {
		err := json.Unmarshal(buf, &params)
		if err != nil {
			log.Fatal(err)
		}
	}
	if params.Wheelbase == 0 {
		params.Wheelbase = 4.0
	}
	if params.GameWheelToTire == 0 {
		params.GameWheelToTire = 1.0
	}
	if params.TireMax == 0 {
		params.TireMax = math.Pi / 4
	}
	// We need to tune DtireDtMax experimentally.

	// Don't allow configuring this from JSON
	if params.Interval != defaultInterval {
		log.Printf("force planner interval to %d, not %v", defaultInterval, params.Interval)
		params.Interval = defaultInterval
	}

	return params
}


// Probably need something better, like a PI controller.
func intervalController(setpoint time.Duration, tickPrev, tickNow time.Time) time.Duration {
	actual := tickNow.Sub(tickPrev).Microseconds()
	intervalErr := actual - setpoint.Microseconds()
	adjusted := time.Duration(setpoint.Microseconds()-intervalErr) * time.Microsecond
	return adjusted
}

// Make a Speed class from the TPV message.
func tpvToSpeed(buf []byte) *Speed {
	var tpv gpsd.TPV
	err := json.Unmarshal(buf, &tpv)
	if err != nil {
		log.Fatal(err)
	}
	const classTPV = "TPV"
	if tpv.Class != classTPV {
		log.Fatal(errors.New(fmt.Sprintf("expected class %s, got %s", classTPV, tpv.Class)))
	}
	if tpv.Mode < gpsd.Mode2D {
		log.Printf("ignoring GPSD message with mode %d", tpv.Mode)
		return nil
	}
	return &Speed{Class: "SPEED",
		Gpsd:     tpv.Time.UnixMicro(),
		Received: time.Now().UnixMicro(),
		Speed:    tpv.Speed}

}

// Make Trajectories from speed and g920.
func Planner(param PlannerParameters, gpsdCh <-chan []byte, g920Ch <-chan g920.G920,
	logging bool) (<-chan []byte, <-chan []byte, <-chan []byte) {
	trajectories := make(chan []byte)
	vehicleCommands := make(chan []byte)
	var logCh chan []byte
	if logging {
		logCh = make(chan []byte)
	}

	intervalSetpoint := time.Duration(param.Interval) * time.Millisecond
	trajectoryInterval := intervalSetpoint

	// State variables
	var report g920.G920     // most recent from channel
	var speed Speed          // most recent
	var tirePrev float64     // at last trajectory

	ticker := time.NewTicker(trajectoryInterval)
	// tickPrev := time.Now()

	go func() {
		defer ticker.Stop()
		defer close(trajectories)
		defer close(vehicleCommands)
		if logCh != nil {
			defer close(logCh)
		}
		for {
			select {
			case tpv, ok := <-gpsdCh:
				if !ok {
					log.Printf("GPSD channel closed")
					return
				}
				if maybe := tpvToSpeed(tpv); maybe != nil {
					speed = *maybe
				}
			case report_, okG920 := <-g920Ch:
				if !okG920 {
					log.Printf("G920 channel closed")
					return
				}
				// There might be something wrong with us resolution clocks on Windows.
				if report_.Requested < report.Requested {
					log.Printf("out-of-order G920 reports at %d < %d", report_.Requested, report.Requested)
					break
				}
				report = report_
			case <-ticker.C:
				tireRequested := param.tire(report.Wheel)
				tireLimited := param.limitDtireDt(tirePrev, tireRequested, intervalSetpoint.Seconds())
				// fmt.Printf("report wheel %.2f, tire requested %.2f, limited %.2f\n", report.Wheel, tireRequested, tireLimited)
				curvature := param.curvature(tireLimited)

				// We had a bug; be sure it doesn't come back.
				if math.Abs(curvature) > 6000 {
					log.Printf("tireRequested %f, tirePrev %f, tireLimited %f", tireRequested, tirePrev, tireLimited)
				}
				trajectory := Trajectory{Class: ClassTrajectory,
					Requested: time.Now().UnixMicro(),
					Curvature: curvature,
					Speed:     speed.Speed}
				tb := trajectory.Bytes()
				trajectories <- tb
				vehicleCommands <- tb
				vehicleCommands <- report.Bytes() // include G920 report for braking and accelerator
				if logCh != nil {
					select {
					case logCh <- speed.bytes():
					default:
						log.Print("log channel not ready, packet dropped (speed)")
					}

					select {
					case logCh <- report.Bytes():
					default:
						log.Print("log channel not ready, packet dropped (g920)")
					}

					select {
					case logCh <- trajectory.Bytes():
					default:
						log.Print("log channel not ready, packet dropped (trajectory)")
					}
				}
				tirePrev = tireLimited

				/*
					        tickNow := time.Now()
						adjusted := intervalController(intervalSetpoint, tickPrev, tickNow)
						ticker.Reset(adjusted)
						tickPrev = tickNow
						trajectoryInterval = adjusted
				*/
			}
		}
	}()
	return trajectories, vehicleCommands, logCh
}
