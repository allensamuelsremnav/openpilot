// Offline GNSS_CLIENT.
package main

import (
	"flag"
	"fmt"
	"html/template"
	"io"
	"log"
	"os"
	"path/filepath"

	machineid "github.com/denisbrodbeck/machineid"
	gpsd "remnav.com/remnav/metadata/gpsd"
	storage "remnav.com/remnav/metadata/storage"
)

func fileTransfer(machineID, gnssSubdir, localRoot, archiveServer, archiveRoot string, script io.Writer) {
	// Write bash script to copy local session storage.
	m := map[string]interface{}{
		"machineID":     machineID,
		"gnssSubdir":    gnssSubdir,
		"localRoot":     localRoot,
		"archiveServer": archiveServer,
		"archiveRoot":   archiveRoot}

	t := `#!/bin/bash
# Run this script after session is complete
# and a reliable TCP connection is available.
#    bash filetransfer.sh hjeng
if [ "$#" -ne 1 ]; then
  echo "archive user id required, e.g. hjeng"
  exit 1
fi
ARCHIVE_USER="$1"
ARCHIVE_SERVER="{{.archiveServer}}"
ARCHIVE_ROOT="{{.archiveRoot}}"
GNSS_SUBDIR="{{.gnssSubdir}}"
LOCAL_ROOT="{{.localRoot}}"
MACHINE_ID="{{.machineID}}"
rsync -arv ${LOCAL_ROOT}/${GNSS_SUBDIR}/${MACHINE_ID} ${ARCHIVE_USER}@${ARCHIVE_SERVER}:${ARCHIVE_ROOT}/${GNSS_SUBDIR}

`
	t_ := template.Must(template.New("").Parse(t))
	t_.Execute(script, m)
}

func main() {
	// Listen to gpsd and write messages to
	// <vehicle_root>/gnss/<machine_id>/<YYYYmmddTHHMM_<fmtid>.json>
	// ./gpsdol
	vehicleRootFlag := flag.String("vehicle_root",
		"/home/user/6TB/vehicle/remconnect",
		"vehicle storage directory")
	archiveServerFlag := flag.String("archive_server",
		"96.64.247.70",
		"IP address of archive server (e.g. rn3)")
	archiveRootFlag := flag.String("archive_root",
		"/home/user/6TB/remconnect/archive",
		"archive storage directory (e.g. on rn3)")
	gpsdAddressFlag := flag.String("gpsd_address",
		"10.1.10.225:2947",
		"local address of gpsd server")
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s [OPTIONS]\n", os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()
	if flag.NArg() > 0 {
		log.Printf("ignoring unexpected argument %s\n", flag.Arg(0))
	}

	protectedID, err := machineid.ProtectedID("gpsdol")
	if err != nil {
		log.Fatalln(err)
	}

	log.Printf("%s: machine id %s\n", os.Args[0], protectedID)

	// Make the local gnss storage directory if it doesn't exist.
	gnssPath := filepath.Join(*vehicleRootFlag, storage.RawGNSSSubdir, protectedID)
	err = os.MkdirAll(gnssPath, 0775)
	if err != nil {
		log.Fatalf("%s: %s while creating directory %s",
			os.Args[0], err, gnssPath)
	}
	// umask modifies the group permission; fix it.
	err = os.Chmod(gnssPath, 0775)
	if err != nil {
		log.Fatal(err)
	}

	// Write script to transfer local session files.
	scriptFilepath := filepath.Join(gnssPath, "filetransfer.sh")
	scriptFile, err := os.Create(scriptFilepath)
	if err != nil {
		log.Fatal(err)
	}
	defer scriptFile.Close()
	fileTransfer(protectedID,
		storage.RawGNSSSubdir,
		*vehicleRootFlag,
		*archiveServerFlag, *archiveRootFlag,
		scriptFile)
	os.Chmod(scriptFilepath, 0775)
	log.Printf("%s: run this script on vehicle when session is finished and a reliable WiFi or Ethernet connection is available: %s\n",
		os.Args[0],
		scriptFilepath)

	// Set up the connection to gpsd.
	conn, reader := gpsd.Conn(*gpsdAddressFlag)
	defer conn.Close()

	gpsd.PokeWatch(conn)

	gpsd.WatchBinned(*gpsdAddressFlag, reader, gnssPath)
}
