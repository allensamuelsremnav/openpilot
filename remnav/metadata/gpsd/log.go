package gpsd

import (
	"html/template"
	"io"
	"log"
	"os"
	"path/filepath"

	machineid "github.com/denisbrodbeck/machineid"
	storage "remnav.com/remnav/metadata/storage"
)

func fileTransfer(machineID, gnssSubdir, localRoot, archiveServer, archiveRoot string, script io.Writer) {
	// Write bash script to copy local session storage.
	// Uses rsync, so the archive server must be Linuxy.
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

// Initialize the log storage and return path to directory.
func LogDir(tag, vehicleRoot, archiveServer, archiveRoot string) string {
	// tag identifies source, e.g. gpsdol or gpsdrt
	protectedID, err := machineid.ProtectedID(tag)
	if err != nil {
		log.Fatalln(err)
	}

	log.Printf("%s: machine id %s\n", tag, protectedID)

	// Make the local gnss storage directory if it doesn't exist.
	gnssPath := filepath.Join(vehicleRoot, storage.RawGNSSSubdir, protectedID)
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

	if len(archiveServer) != 0 {
		// Write script to transfer local session files.
		scriptFilepath := filepath.Join(gnssPath, "filetransfer.sh")
		scriptFile, err := os.Create(scriptFilepath)
		if err != nil {
			log.Fatal(err)
		}
		defer scriptFile.Close()
		fileTransfer(protectedID,
			storage.RawGNSSSubdir,
			vehicleRoot,
			archiveServer, archiveRoot,
			scriptFile)
		os.Chmod(scriptFilepath, 0775)
		log.Printf("%s: run this script on vehicle when session is finished and a reliable WiFi or Ethernet connection is available: %s\n",
			os.Args[0],
			scriptFilepath)
	}

	return gnssPath
}
