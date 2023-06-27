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
	sin := math.Sin(tire)
	// Don't divide by zero
	const eps = 1e-300
	if math.Abs(sin) < eps {
		sin = math.Copysign(eps, sin)
	}
	return p.Wheelbase / sin
}

// Probably need something better, like a PI controller.
func intervalController(setpoint time.Duration, tickPrev, tickNow time.Time) time.Duration {
	actual := tickNow.Sub(tickPrev).Microseconds()
	intervalErr := actual - setpoint.Microseconds()
	adjusted := time.Duration(setpoint.Microseconds()-intervalErr) * time.Microsecond
	return adjusted
}

// Make a Speed class from the TPV message.
func tpvToSpeed(buf []byte) Speed {
	var tpv gpsd.TPV
	err := json.Unmarshal(buf, &tpv)
	if err != nil {
		log.Fatal(err)
	}
	const classTPV = "TPV"
	if tpv.Class != classTPV {
		log.Fatal(errors.New(fmt.Sprintf("expected class %s, got %s", classTPV, tpv.Class)))
	}
	return Speed{Class: "SPEED",
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
	var report g920.G920     // most recent
	var reportPrev g920.G920 // at last trajectory
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
		var okG920 bool
		for {
			select {
			case tpv, ok := <-gpsdCh:
				if !ok {
					log.Printf("GPSD channel closed")
					return
				}
				speed = tpvToSpeed(tpv)
			case report, okG920 = <-g920Ch:
				if !okG920 {
					log.Printf("G920 channel closed")
					return
				}
			case <-ticker.C:
				tireRequested := param.tire(report.Wheel)
				const microsToSeconds = 1e-6
				dt := float64(report.Requested-reportPrev.Requested) * microsToSeconds
				if dt <= 0 {
					log.Printf("dt <= 0 for G920 at %d - %d", report.Requested, reportPrev.Requested)
					continue
				}
				tireLimited := param.limitDtireDt(tirePrev, tireRequested, dt)
				// fmt.Printf("report wheel %.2f, tire requested %.2f, limited %.2f\n", report.Wheel, tireRequested, tireLimited)
				curvature := param.curvature(tireLimited)

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
				reportPrev = report

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
