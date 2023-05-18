package trajectory

import (
	"encoding/json"
	"errors"
	"fmt"
	"log"
	"time"
)

const ClassTrajectory = "TRAJECTORY"

type Trajectory struct {
	Class     string  `json:"class"`
	Requested int64   `json:"requested"`  // μs since Unix epoch
	Curvature float64 `json':"curvature"` // 1/m
	Speed     float64 `json':"speed"`     // m/s
}

const ClassTrajectoryApplication = "TRAJECTORY_APPLICATION"

type TrajectoryApplication struct {
	Class      string `json:"class"`
	Trajectory int64  `json:"trajectory"` // μs since Unix epoch
	Applied    int64  `json:"applied"`    // μs since Unix epoch
	// Log the time received at the listener.
	Log int64 `json:"log"` // μs since Unix epoch
}

type tsProbe struct {
	Class     string `json:"class"`
	Requested int64  `json:"requested"` // μs since Unix epoch
	Log       int64  `json:"log"`       // μs since Unix epoch}
}

// Extract the timestamp for a Trajectory or TrajectoryApplication.
func Timestamp(msg []byte) (int64, error) {
	var probe tsProbe
	err := json.Unmarshal(msg, &probe)
	if err != nil {
		return 0, err
	}
	if probe.Class == ClassTrajectory {
		return probe.Requested, nil
	} else if probe.Class == ClassTrajectoryApplication {
		return probe.Log, nil
	}
	return 0, errors.New(fmt.Sprintf("unexpected class %s", probe.Class))
}

// Return a channel of deduped messages with log time.
func Dedup(recvd <-chan []byte, progress, verbose bool) <-chan []byte {
	deduped := make(chan []byte)
	go func() {
		defer close(deduped)
		var latest int64
		for msg := range recvd {
			var applied TrajectoryApplication
			err := json.Unmarshal(msg, &applied)
			if err != nil {
				log.Fatal(err)
			}
			if applied.Class != ClassTrajectoryApplication {
				log.Fatal(errors.New(fmt.Sprintf("expected class %s, got %s",
					ClassTrajectoryApplication, applied.Class)))
			}
			if applied.Trajectory > latest {
				if progress {
					fmt.Printf("a")
				}
				latest = applied.Trajectory
				applied.Log = time.Now().UnixMicro()
				annotated, _ := json.Marshal(applied)
				if verbose {
					fmt.Println(string(annotated))
				}
				deduped <- annotated
			}
		}
	}()
	return deduped
}
