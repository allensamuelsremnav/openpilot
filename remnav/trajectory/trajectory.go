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

func (t Trajectory) Bytes() []byte {
	bytes, err := json.Marshal(t)
	if err != nil {
		log.Fatal(err)
	}
	return bytes
}
func (t Trajectory) Timestamp() (int64, error) {
	return t.Requested, nil
}

func (t Trajectory) String() string {
	return t.String()
}

const ClassTrajectoryApplication = "TRAJECTORY_APPLICATION"

type TrajectoryApplication struct {
	Class      string `json:"class"`
	Trajectory int64  `json:"trajectory"` // μs since Unix epoch
	Applied    int64  `json:"applied"`    // μs since Unix epoch
	// Log the time received at the listener.
	Log int64 `json:"log"` // μs since Unix epoch
}

func (a TrajectoryApplication) Bytes() []byte {
	bytes, err := json.Marshal(a)
	if err != nil {
		log.Fatal(err)
	}
	return bytes
}

func (a TrajectoryApplication) Timestamp() (int64, error) {
	return a.Log, nil
}

func (a TrajectoryApplication) String() string {
	return string(a.Bytes())
}

// Return a channel of deduped application messages with log time.
func Dedup(recvd <-chan []byte, progress, verbose bool) <-chan TrajectoryApplication {
	deduped := make(chan TrajectoryApplication)
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
				if verbose {
					fmt.Println(applied.String())
				}
				deduped <- applied
			}
		}
	}()
	return deduped
}
