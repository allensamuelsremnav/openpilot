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
	Requested int64   `json:"requested"` // μs since Unix epoch.  Unique identifier.
	Curvature float64 `json:"curvature"` // 1/m; positive left turn, counterclockwise turn
	Speed     float64 `json:"speed"`     // m/s
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
	return string(t.Bytes())
}

const ClassTrajectoryApplication = "TRAJECTORY_APPLICATION"

// The TrajectoryApplication message reports the first time that a
// Trajectory request affects the Remnav video.  It is used to
// distinguish applied vs in-flight trajectory requests for the
// operator display.
type TrajectoryApplication struct {
	Class      string `json:"class"`
	Trajectory int64  `json:"trajectory"` // Trajectory.Requested field.
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
				if verbose {
					fmt.Println(applied.String())
				}
				bytes, err := json.Marshal(applied)
				if err != nil {
					log.Fatal(err)
				}
				deduped <- bytes
			}
		}
	}()
	return deduped
}
