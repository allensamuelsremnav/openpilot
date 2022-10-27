// Offline GNSS_CLIENT.
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"path/filepath"
	"time"

	"github.com/google/uuid"
	experiment "remnav.com/remnav/metadata/experiment"
	gpsd "remnav.com/remnav/metadata/gpsd"
	storage "remnav.com/remnav/metadata/storage"
)

func main() {
	// Listen to gpsd and write messages to
	// vehicle_root/session_id/gnss/<YYYYmmddTHHMMSST_<fmtid>.json>
	// ./gpsdol --session_id <session_id> --config <experiment.json>
	sessionIdFlag := flag.String("session_id", "", "specify session id")
	configFlag := flag.String("config", "", "JSON experiment configuration")
	flag.Parse()
	if len(*configFlag) == 0 {
		log.Fatalf("%s: --config required", os.Args[0])
	}

	var sessionId string
	if len(*sessionIdFlag) > 0 {
		sessionId = *sessionIdFlag
	} else {
		// Convenience for testing.
		sessionId = fmt.Sprintf("%s_%s",
			time.Now().UTC().Format("20060102T150405Z"),
			uuid.NewString())
	}
	log.Printf("%s: session_id %s", os.Args[0], sessionId)

	// Read the configuration JSON.
	configPath, err := filepath.Abs(*configFlag)
	if err != nil {
		log.Fatalln(err)
	}
	log.Printf("%s: config file %s", os.Args[0], configPath)
	config, _ := experiment.Read(configPath)

	if len(config.Description) > 0 {
		log.Printf("%s: configuration description \"%s\"",
			os.Args[0], config.Description)
	}

	// Check config values.
	if len(config.GNSS.GPSDAddress) == 0 {
		log.Fatalf("%s: gpsd_address invalid or not found in %s",
			os.Args[0], configPath)
	}
	if len(config.Storage.VehicleRoot) == 0 {
		log.Fatalf("%s: vehicle_root invalid or not found in %s",
			os.Args[0], configPath)
	}
	info, err := os.Stat(config.Storage.VehicleRoot)
	if os.IsNotExist(err) || !info.IsDir() {
		log.Fatalf("%s: vehicle_root %s not found or not a directory",
			os.Args[0], config.Storage.VehicleRoot)
	}

	// Make the session directory and GNSS subdirectory.
	sessionPath := filepath.Join(config.Storage.VehicleRoot, sessionId)
	GNSSPath := filepath.Join(sessionPath, storage.GNSSSubdir)
	err = os.MkdirAll(GNSSPath, 0775)
	if err != nil {
		log.Fatalln(err)
	}
	// umask modifies the group permission; fix it.
	err = os.Chmod(sessionPath, 0775)
	if err != nil {
		log.Fatalln(err)
	}

	err = os.Chmod(GNSSPath, 0775)
	if err != nil {
		log.Fatalln(err)
	}

	// Make the output file.
	fmtId := "g000"
	timestampStr := time.Now().UTC().Format("20060102T150405Z")
	GNSSFilename := fmt.Sprintf("%s_%s.json",
		timestampStr,
		fmtId)
	GNSSFile, err := os.Create(filepath.Join(GNSSPath, GNSSFilename))
	if err != nil {
		log.Fatalln(err)
	}

	// Make connection and reader.
	log.Printf("%s: connecting to %s", os.Args[0], config.GNSS.GPSDAddress)
	conn, err := net.Dial("tcp4", config.GNSS.GPSDAddress)
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()
	reader := bufio.NewReader(conn)
	line, err := reader.ReadString('\n')
	if err != nil {
		log.Fatal(err)
	}
	// This should be the gpsd version information.
	log.Println(line)

	gpsd.PokeWatch(conn)

	// Gather all of the input
	deviceCheck := true
	lineCount := 0
	for {
		line, err := reader.ReadString('\n')
		if err == nil {
			if deviceCheck {
				// Misconfiguration could point to a gpsd server with no devices.
				var probe gpsd.Class
				err := json.Unmarshal([]byte(line), &probe)
				if err != nil {
					log.Fatal(err)
				}
				if probe.Class == "DEVICES" {
					var devices gpsd.Devices
					err := json.Unmarshal([]byte(line), &devices)
					if err != nil {
						log.Fatal(err)
					}
					if len(devices.Devices) == 0 {
						log.Fatalf("no devices found for gpsd at %s.  is a GPS attached?",
							config.GNSS.GPSDAddress)
					}
					deviceCheck = false
				}
			}
			_, err = fmt.Fprint(GNSSFile, line)
			if err != nil {
				log.Fatal(err)
			}
			lineCount++
			if lineCount%500 == 0 {
				log.Printf("%s: %d messages", os.Args[0], lineCount)
			}
		} else if err == io.EOF {
			break
		} else {
			log.Fatal(err)
		}
	}
}
