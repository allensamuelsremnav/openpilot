package trajectory

import (
	"encoding/json"
	"log"
	"time"
)

// Vehicle mock takes Trajectories and computes TrajectoryApplied messages with
// realistic RTT  and application delays.

// RTT is modeled as
// operator -> vehicle
// + start control computation (sync with OpenPilot)
// + estimate application time
// + vehicle -> operator
//
// The first applied message arrives at the operator station after an RTT;
// on average, subsequent messages arrive at 20 Hz.
//
// A rough estimate might be 30 + 50 + 0 + 35 (on average)
//
// Application delay is
// operator -> vehicle
// + start control computation (sync with OpenPilot)
// + compute control
// + sync with OpenPilot controls
// + control delay, e.g. actuators
//
// A guess is 30 + 50/2 + 50/2 + 50/2 + 50
//
// The vehicle control loop executes at 20 Hz
type VehicleMock struct {
	RTT              int // ms
	ApplicationDelay int // ms
	ExecutionPeriod  int // ms
}

func (m VehicleMock) apply(trajBytes []byte) []byte {
	var traj Trajectory
	err := json.Unmarshal(trajBytes, &traj)
	if err != nil {
		log.Fatal(err)
	}
	var appl TrajectoryApplication
	appl.Class = ClassTrajectoryApplication
	appl.Trajectory = traj.Requested
	appl.Applied = (int64)(m.ApplicationDelay)*1000 + traj.Requested
	applBytes, err := json.Marshal(appl)
	if err != nil {
		log.Fatal(err)
	}
	return applBytes
}

// Generate TrajectoryApplied messages for a stream of Trajectory messages.
func (m VehicleMock) run(trajectories <-chan []byte) <-chan []byte {
	applicationCh := make(chan []byte)

	executionDuration := time.Duration(m.ExecutionPeriod) * time.Millisecond

	go func() {
		defer close(applicationCh)
		var queue [][]byte
		queue = append(queue, <-trajectories)
		// Force RTT delay for first delivered TrajectoryApplication.
		startTimer := time.NewTimer(time.Duration(m.RTT) * time.Millisecond)
	startLoop:
		for {
			select {
			case traj := <-trajectories:
				queue = append(queue, traj)
			case <-startTimer.C:
				tb := queue[0]
				queue = queue[1:]
				applicationCh <- m.apply(tb)
				break startLoop
			}
		}

		// Force subsequent reports every ExecutionPeriod.
		var executionTimer = time.NewTimer(executionDuration)
		trajOk := true
		for {
			var traj []byte
			select {
			case traj, trajOk = <-trajectories:
				if !trajOk {
					trajectories = nil
					break
				}
				queue = append(queue, traj)
			case <-executionTimer.C:
				if len(queue) > 0 {
					tb := queue[0]
					queue = queue[1:]
					applicationCh <- m.apply(tb)
					executionTimer.Reset(executionDuration)
				}
				if len(queue) == 0 && !trajOk {
					return
				}
			}
		}
	}()
	return applicationCh
}
