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

// Generate TrajectoryApplication with delayed application time.
func (m VehicleMock) apply(traj Trajectory) []byte {
	if traj.Class != ClassTrajectory {
		log.Fatalf("expected class %s, go %s", ClassTrajectory, traj.Class)
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

// Convert cmd to Trajectory if possible.
func maybeTrajectory(cmd []byte) *Trajectory {
	var probe Trajectory
	err := json.Unmarshal(cmd, &probe)
	if err != nil {
		log.Fatal(err)
	}
	if probe.Class == ClassTrajectory {
		return &probe
	}
	return nil
}

// Generate TrajectoryApplied messages for a stream of commands.
func (m VehicleMock) Run(cmds <-chan []byte) <-chan []byte {
	applicationCh := make(chan []byte)

	executionDuration := time.Duration(m.ExecutionPeriod) * time.Millisecond

	go func() {
		defer close(applicationCh)
		var queue []Trajectory
		for {
			if maybe := maybeTrajectory(<-cmds); maybe != nil {
				queue = append(queue, *maybe)
				break
			}
		}
		// Force RTT delay for first delivered TrajectoryApplication.
		startTimer := time.NewTimer(time.Duration(m.RTT) * time.Millisecond)
	startLoop:
		for {
			select {
			case cmd := <-cmds:
				if maybe := maybeTrajectory(cmd); maybe != nil {
					queue = append(queue, *maybe)
				}
			case <-startTimer.C:
				tb := queue[0]
				queue = queue[1:]
				applicationCh <- m.apply(tb)
				break startLoop
			}
		}

		// Force subsequent reports every ExecutionPeriod.
		var executionTicker = time.NewTicker(executionDuration)
		cmdOk := true
		for {
			var cmd []byte
			select {
			case cmd, cmdOk = <-cmds:
				if !cmdOk {
					cmds = nil
					break
				}
				if maybe := maybeTrajectory(cmd); maybe != nil {
					queue = append(queue, *maybe)
				}
			case <-executionTicker.C:
				if len(queue) > 0 {
					t := queue[0]
					queue = queue[1:]
					applicationCh <- m.apply(t)
				}
				if len(queue) == 0 && !cmdOk {
					return
				}
			}
		}
	}()
	return applicationCh
}
