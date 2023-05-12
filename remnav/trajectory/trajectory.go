package trajectory

const ClassTrajectory = "TRAJECTORY"

type Trajectory struct {
	Class     string  `json:"class"`
	Requested int64   `json:"requested"`  // μs since Unix epoch
	Curvature float64 `json':"curvature"` // 1/m
	Speed     float64 `json':"speed"`     // m/s
	Log       int64   `json:"log"`        // μs since Unix epoch}
}

const ClassTrajectoryApplication = "TRAJECTORY_APPLICATION"

type TrajectoryApplication struct {
	Class      string `json:"class"`
	Trajectory int64  `json:"trajectory"` // μs since Unix epoch
	Applied    int64  `json:"applied"`    // μs since Unix epoch
	Log        int64  `json:"log"`        // μs since Unix epoch}
}
