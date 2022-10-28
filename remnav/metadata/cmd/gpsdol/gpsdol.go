// Offline GNSS_CLIENT.
package main

import (
	"flag"
	"fmt"
	"log"
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

	conn, reader := gpsd.Conn(config.GNSS.GPSDAddress)
	defer conn.Close()

	gpsd.PokeWatch(conn)

	gpsd.Watch(config.GNSS.GPSDAddress, reader, GNSSFile)
}
