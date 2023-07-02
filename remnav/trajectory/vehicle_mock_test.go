package trajectory

import (
	"encoding/json"
	"fmt"
	"math"
	"testing"
	"time"
)

func TestVehicleMock(t *testing.T) {
	var mock VehicleMock
	mock.RTT = 115
	mock.ApplicationDelay = 205
	mock.ExecutionPeriod = 50
	trajectories := make(chan []byte)
	applications := mock.Run(trajectories)
	go func(n int) {
		defer close(trajectories)
		ticker := time.NewTicker(time.Duration(50) * time.Millisecond)
		for i := 0; i < 10; i++ {
			select {
			case <-ticker.C:
				var traj Trajectory
				traj.Class = ClassTrajectory
				traj.Requested = time.Now().UnixMicro()
				bytes, err := json.Marshal(traj)
				if err != nil {
					t.Fatal(err)
				}
				trajectories <- bytes
			}
		}
	}(10)
	for applBytes := range applications {
		now := time.Now().UnixMicro()
		var appl TrajectoryApplication
		err := json.Unmarshal(applBytes, &appl)
		if err != nil {
			t.Error(err)
		}
		fmt.Printf("RTT %d μs = %d - %d, observed applied - trajectory = %d μs = %d - %d\n", now-appl.Trajectory, now, appl.Trajectory, appl.Applied-appl.Trajectory, appl.Applied, appl.Trajectory)
		eps := 7500.0*2
		nominal := int64(mock.RTT * 1000)
		checkRTT := math.Abs((float64)(now - appl.Trajectory - nominal))
		if checkRTT > eps {
			t.Fatalf("RTT error %f > %f, observed RTT now - appl.Trajectory = %d μs = %d - %d, nominal %d μs",
				checkRTT, eps,
				now-appl.Trajectory, now, appl.Trajectory, nominal)
		}
		checkDelay := math.Abs((float64)(appl.Applied - appl.Trajectory - int64(mock.ApplicationDelay*1000)))
		epsDelay := 0.0 // Delay is fixed.
		if checkDelay > epsDelay {
			t.Fatalf("%f, appl.Applied-appl.Trajectory %d = %d - %d > %f", checkDelay, appl.Applied-appl.Trajectory, appl.Applied, appl.Trajectory, epsDelay)
		}

	}
}
