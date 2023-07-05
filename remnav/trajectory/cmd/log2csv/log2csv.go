// Convert vehiclecmd log to csv files.
package main
import (
	"bufio"
	"encoding/csv"
	"encoding/json"
	"flag"
	"log"
	"os"
	"strconv"
	
	"remnav.com/remnav/g920"
	"remnav.com/remnav/metadata/storage"
	"remnav.com/remnav/trajectory"
)

// Scan vehiclecmd log file and  write messages to separate csv files.
func run(logScanner *bufio.Scanner, g920Writer, trajWriter *csv.Writer) {
	defer g920Writer.Flush()
	defer trajWriter.Flush()

	for logScanner.Scan() {
		raw := logScanner.Text()
		// Use a g920 struct for probe.
		var probe g920.G920
		err := json.Unmarshal([]byte(raw), &probe)
		if err != nil {
			log.Fatal(err)
		}

		if probe.Class == g920.ClassG920 {
			requested := strconv.FormatInt(probe.Requested, 10)
			wheel := strconv.FormatFloat(probe.Wheel, 'g', -1, 64)
			pedalmiddle := strconv.Itoa(g920.PedalMiddle)
			pedalright := strconv.Itoa(g920.PedalRight)
			g920Writer.Write([]string{requested, wheel, pedalmiddle, pedalright})
		} else if probe.Class == trajectory.ClassTrajectory {
			// Need to unmarshal to the correct class.
			var trajmsg trajectory.Trajectory
			err = json.Unmarshal([]byte(raw), & trajmsg)
			if err != nil {
				log.Fatal(err)
			}
			requested := strconv.FormatInt(trajmsg.Requested, 10)
			curvature := strconv.FormatFloat(trajmsg.Curvature, 'g', -1, 64)
			speed := strconv.FormatFloat(trajmsg.Speed, 'g', -1, 64)
			trajWriter.Write([]string{requested, curvature, speed})
		} else {
			log.Fatalf("unexpected class %s", probe.Class)
		}
	}
	if err := logScanner.Err(); err != nil {
		log.Fatal(err)
	}
}

// Scan vehiclecmd log file and  write messages to separate csv files.
func run_traj(logScanner *bufio.Scanner, trajWriter, trajapplWriter *csv.Writer) {
	defer trajWriter.Flush()
	defer trajapplWriter.Flush()

	for logScanner.Scan() {
		raw := logScanner.Text()
		// Use an aplicationstruct for probe.
		var probe trajectory.TrajectoryApplication
		err := json.Unmarshal([]byte(raw), &probe)
		if err != nil {
			log.Fatal(err)
		}

		if probe.Class == trajectory.ClassTrajectoryApplication {
			trajectory := strconv.FormatInt(probe.Trajectory, 10)
			applied := strconv.FormatInt(probe.Applied, 10)
			delay := strconv.FormatInt(probe.Applied - probe.Trajectory, 10)
			trajapplWriter.Write([]string{trajectory, applied, delay})
		} else if probe.Class == trajectory.ClassTrajectory {
			// Need to unmarshal to the correct class.
			var trajmsg trajectory.Trajectory
			err = json.Unmarshal([]byte(raw), & trajmsg)
			if err != nil {
				log.Fatal(err)
			}
			requested := strconv.FormatInt(trajmsg.Requested, 10)
			curvature := strconv.FormatFloat(trajmsg.Curvature, 'g', -1, 64)
			speed := strconv.FormatFloat(trajmsg.Speed, 'g', -1, 64)
			trajWriter.Write([]string{requested, curvature, speed})
		} else {
			log.Fatalf("unexpected class %s", probe.Class)
		}
	}
	if err := logScanner.Err(); err != nil {
		log.Fatal(err)
	}
}

// Identify the log file and output files.
func main() {
	defid := "e3af1220630ed7df311fb6c324d3ab91481da09652d747c9b9b8bb219972b778"
	machineid := flag.String(
		"machine_id",
		defid,
		"machine id, e.g. " + defid)
	outroot := flag.String("out_root", ".", "directory for output csv")
	logroot := flag.String("log_root", "C:/remnav_log", "root directory for log files")
	logtype := flag.String("log_type",
		storage.VehicleCmdSubdir,
		storage.VehicleCmdSubdir + " or " + storage.TrajectorySubdir)
	deflog := "20230702T0033Z"
	logfilename := flag.String("log", deflog, "e.g. " + deflog)
	flag.Parse()

	// Prepare log scanner.
	logpath := *logroot + "/" + *logtype + "/" + *machineid + "/" + *logfilename
	logfile, err := os.Open(logpath)
	if err != nil {
		log.Fatal(err)
	}
	defer logfile.Close()
	logScanner := bufio.NewScanner(logfile)
	
	if *logtype == storage.VehicleCmdSubdir {
		// Prepare g920 csv writer
		g920out, err := os.Create(*outroot + "/" + "g920.csv")
		if err != nil {
			log.Fatal(err)
		}
		defer g920out.Close()
		g920Writer := csv.NewWriter(g920out)
		g920Writer.Write([]string{"requested", "wheel", "pedalmiddle", "pedalright"})
	
		// Prepare trajectory csv writer
		trajout, err := os.Create(*outroot + "/" + "traj.csv")
		if err != nil {
			log.Fatal(err)
		}
		defer trajout.Close()
		trajWriter := csv.NewWriter(trajout)
		trajWriter.Write([]string{"requested", "curvature", "speed"})
		
		run(logScanner, g920Writer, trajWriter)
	} else if *logtype == storage.TrajectorySubdir {
	
		// Prepare trajectory csv writer
		trajout, err := os.Create(*outroot + "/" + "traj.csv")
		if err != nil {
			log.Fatal(err)
		}
		defer trajout.Close()
		trajWriter := csv.NewWriter(trajout)
		trajWriter.Write([]string{"requested", "curvature", "speed"})
		
		// Prepare trajectory application csv writer
		trajapplout, err := os.Create(*outroot + "/" + "trajapplication.csv")
		if err != nil {
			log.Fatal(err)
		}
		defer trajapplout.Close()
		trajapplWriter := csv.NewWriter(trajapplout)
		trajapplWriter.Write([]string{"trajectory", "applied","delay"})

		run_traj(logScanner, trajWriter, trajapplWriter)

	} else {
		log.Fatalf("unexpected logtype %s", *logtype)
	}

}

