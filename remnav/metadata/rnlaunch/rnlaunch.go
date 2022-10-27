package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"sync"
	"text/template"
	"time"

	experiment "remnav.com/remnav/metadata/experiment"

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

func fileTransfer(localRoot, archiveServer, archiveRoot, sessionId string, script io.Writer) {
	// Write bash script to copy local session storage.
	m := map[string]interface{}{
		"localRoot":     localRoot,
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
LOCAL_ROOT="{{.localRoot}}"
rsync -arv ${LOCAL_ROOT}/${SESSION_ID} ${ARCHIVE_USER}@${ARCHIVE_SERVER}:${ARCHIVE_ROOT}

`
	t_ := template.Must(template.New("").Parse(t))
	t_.Execute(script, m)
}

func main() {
	sessionIdFlag := flag.String("session_id", "", "specify session id rather use automatic UUID")
	videoSenderFlag := flag.String("video_sender", "", "override video_sender in configuration")
	GNSSClientFlag := flag.String("gnss_client", "", "override gnss_client in configuration")
	flag.Parse()
	if len(flag.Args()) != 1 {
		log.Fatalln("expected experiment configuration argument, got args", flag.Args())
	}

	configPath, err := filepath.Abs(flag.Args()[0])
	if err != nil {
		log.Fatalln(err)
	}
	log.Println("config file", configPath)
	config, configBytes := experiment.Read(configPath)

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
	if len(*GNSSClientFlag) > 0 {
		// flag overrides configuration value
		GNSSClient = *GNSSClientFlag
	}
	if len(GNSSClient) > 0 {
		log.Println("gnss_client", GNSSClient)
	}
	// launcher doesn't directly use this, but it's easy to get wrong.
	if len(config.GNSS.GPSDAddress) == 0 {
		log.Fatal("invalid or missing gpsd_address")
	} else {
		log.Println("gpsd_address", config.GNSS.GPSDAddress)
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

	if _, err := os.Stat(config.Storage.VehicleRoot); os.IsNotExist(err) {
		log.Fatal(err)
	}

	localSessionDir := filepath.Join(config.Storage.VehicleRoot, sessionId)
	err = os.Mkdir(localSessionDir, 0775)
	if err != nil && !os.IsExist(err) {
		log.Fatal(err)
	}

	// Copy the config file to the local session dir.
	err = os.WriteFile(filepath.Join(localSessionDir, "experiment.json"),
		configBytes, 0664)
	if err != nil {
		log.Fatal(err)
	}

	// Write script to transfer local session files.
	scriptFilepath := filepath.Join(localSessionDir, "filetransfer.sh")
	scriptFile, err := os.Create(scriptFilepath)
	if err != nil {
		log.Fatal(err)
	}
	defer scriptFile.Close()
	fileTransfer(config.Storage.VehicleRoot,
		config.Storage.ArchiveServer, config.Storage.ArchiveRoot,
		sessionId, scriptFile)
	os.Chmod(scriptFilepath, 0775)
	log.Printf("run this script on vehicle when session is finished and a reliable WiFi or Ethernet connection is available: %s",
		scriptFilepath)

	// Run only video sender until GNSS client is working.
	wg.Add(2)
	go run(videoSender,
		[]string{"--session_id", sessionId, "--config", configPath})
	go run(GNSSClient,
		[]string{"--session_id", sessionId, "--config", configPath})

	wg.Wait()
}
