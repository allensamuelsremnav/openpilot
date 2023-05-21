package trajectory

import (
	"encoding/json"
	"log"
	"math"
	"time"

	g920 "remnav.com/remnav/g920"
	rnlog "remnav.com/remnav/log"
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
type Parameters struct {
	Wheelbase float64 // m
	// Convert game wheel positions to tire angle.
	GameWheel2Tire float64
	// Maximum allowed tire angle
	TireMax float64 // radians
	// Tire steering max angular velocity
	DtireDtMax float64 // radians/s
	// 50 ms at 20 Hz.
	Interval int64 // ms
}

// Convert game wheel to tire angle.
func (p Parameters) tire(gWheel float64) float64 {
	tire := gWheel * p.GameWheel2Tire
	if math.Abs(tire) > p.TireMax {
		tire = math.Copysign(p.TireMax, tire)
	}
	return tire
}

// Limit tire angle rate of change (ω).
// radians and seconds!!!
func (p Parameters) limitDtireDt(tireState, tireRequested, dt float64) float64 {
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
func (p Parameters) curvature(tire float64) float64 {
	sin := math.Sin(tire)
	// Don't divide by zero
	eps := 1e-300
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

// Make a speed class from the TPV message.
func tpv2speed(buf []byte) Speed {
	var tpv gpsd.TPV
	err := json.Unmarshal(buf, &tpv)
	if err != nil {
		log.Fatal(err)
	}
	return Speed{Class: "SPEED",
		Gpsd:     tpv.Time.UnixMicro(),
		Received: time.Now().UnixMicro(),
		Speed:    tpv.Speed}

}
func Planner(param Parameters, gpsdCh <-chan []byte, g920Ch <-chan g920.G920,
	logCh chan<- rnlog.Loggable) <-chan []byte {
	trajectories := make(chan []byte)

	intervalSetpoint := time.Duration(param.Interval) * time.Millisecond
	trajectoryInterval := intervalSetpoint

	// State variables
	var report g920.G920     // most recent
	var reportPrev g920.G920 // at last trajectory
	var speed Speed          // most recent
	var tirePrev float64     // at last trajectory

	ticker := time.NewTicker(trajectoryInterval)
	defer ticker.Stop()
	// tickPrev := time.Now()
	go func() {
		defer close(trajectories)
		var okG920 bool
		for {
			select {
			case tpv, ok := <-gpsdCh:
				if !ok {
					log.Printf("GPSD channel closed")
					return
				}
				speed = tpv2speed(tpv)
			case report, okG920 = <-g920Ch:
				if !okG920 {
					log.Printf("G920 channel closed")
					return
				}
			case <-ticker.C:
				tireRequested := param.tire(report.Wheel)
				dt := float64(report.Requested-reportPrev.Requested) * 1e6
				if dt <= 0 {
					log.Printf("dt <= 0 for G920 at %d", report.Requested)
					continue
				}
				newTire := param.limitDtireDt(tirePrev, tireRequested, dt)
				curvature := param.curvature(newTire)

				trajectory := Trajectory{Class: ClassTrajectory,
					Requested: time.Now().UnixMicro(),
					Curvature: curvature,
					Speed:     speed.Speed}
				trajectories <- trajectory.Bytes()

				logCh <- speed
				logCh <- report
				logCh <- trajectory
				tirePrev = newTire
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
	return trajectories
}
