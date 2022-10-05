package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	experiment "remnav/metadata/experiment"
	"sync"
	"text/template"
	"time"

	"github.com/google/uuid"
)

var wg sync.WaitGroup

func run(prog string, args []string) {
	defer wg.Done()

	log.Printf("exec %s %v", prog, args)
	cmd := exec.Command(prog, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	err := cmd.Start()
	if err != nil {
		log.Fatal(err)
	}
	err = cmd.Wait()
	log.Printf("%s finished with error status %v", prog, err)
}

func fileTransfer(configPath, archiveServer, archiveRoot, sessionId string) {
	m := map[string]interface{}{"configPath": configPath,
		"archiveServer": archiveServer,
		"archiveRoot":   archiveRoot,
		"sessionId":     sessionId}

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
SESSION_ID="{{.sessionId}}"
CONFIG_PATH="{{.configPath}}"
rsync -av ${CONFIG_PATH} ${ARCHIVE_USER}@${ARCHIVE_SERVER}:${ARCHIVE_ROOT}/${SESSION_ID}/
`
	t_ := template.Must(template.New("").Parse(t))
	t_.Execute(os.Stdout, m)
}

func main() {
	sessionIdFlag := flag.String("session_id", "", "specify session id rather use automatic UUID")
	videoSenderFlag := flag.String("video_sender", "", "override video_sender in configuration")
	flag.Parse()
	if len(flag.Args()) != 1 {
		log.Fatalln("expected experiment configuration, got", flag.Args())
	}

	// Read config file.
	configPath, err := filepath.Abs(flag.Args()[0])
	log.Println("config file", configPath)

	configFile, err := os.Open(configPath)
	if err != nil {
		log.Fatalln(err)
	}
	defer configFile.Close()
	byteValue, err := ioutil.ReadAll(configFile)
	if err != nil {
		log.Fatalln(err)
	}

	// Interpret as JSON.
	var config experiment.Config
	json.Unmarshal([]byte(byteValue), &config)

	if len(config.Description) > 0 {
		log.Printf("configuration description \"%s\"",
			config.Description)
	}

	// Look for executables
	videoSender := config.Video.VideoSender
	if len(*videoSenderFlag) > 0 {
		// flag overrides configuration value
		videoSender = *videoSenderFlag
	}
	if len(videoSender) > 0 {
		log.Println("video_sender", videoSender)
	}

	GNSSClient := config.GNSS.GNSSClient
	if len(GNSSClient) > 0 {
		log.Println("gnss_client", GNSSClient)
	}

	var sessionId string
	if len(*sessionIdFlag) > 0 {
		sessionId = *sessionIdFlag
	} else {
		sessionId = fmt.Sprintf("%s_%s",
			time.Now().UTC().Format("20060102T150405Z"),
			uuid.NewString())
	}
	log.Printf("session_id %s (len %d)", sessionId, len(sessionId))

	log.Printf("vehicle_root %s", config.Storage.VehicleRoot)
	log.Printf("archive_server %s", config.Storage.ArchiveServer)
	log.Printf("archive_root %s", config.Storage.ArchiveRoot)

	fileTransfer(configPath, config.Storage.ArchiveServer, config.Storage.ArchiveRoot,
		sessionId)

	// Run only video sender until GNSS client is working.
	wg.Add(1)
	go run(videoSender,
		[]string{"--session_id", sessionId, "--config", configPath})

	wg.Wait()
}
