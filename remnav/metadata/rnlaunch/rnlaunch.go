package main

import (
	"encoding/json"
	"flag"
	"io/ioutil"
	"log"
	"os"
	"os/exec"
	experiment "remnav/metadata/experiment"
	"sync"

	"github.com/google/uuid"
)

var wg sync.WaitGroup

func run(prog string, args []string) {
	defer wg.Done()

	log.Printf("run %s %v", prog, args)
	cmd := exec.Command(prog, args...)
	cmd.Stdout = os.Stdout
	cmd.Stderr = os.Stderr

	err := cmd.Start()
	if err != nil {
		log.Fatal(err)
	}
	err = cmd.Wait()
	log.Printf("%s finished with %v", prog, err)
}

func main() {
	sessionIdFlag := flag.String("session_id", "", "specify session id rather use automatic UUID")
	videoSenderFlag := flag.String("video_sender", "", "override video_sender in configuration")
	flag.Parse()
	if len(flag.Args()) != 1 {
		log.Fatalln("expected experiment configuration, got", flag.Args())
	}

	// Read config file.
	configFilename := flag.Args()[0]
	log.Println("config file", configFilename)
	configFile, err := os.Open(configFilename)
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
		sessionId = uuid.NewString()
	}
	log.Printf("session_id %s (len %d)", sessionId, len(sessionId))

	// Run only video sender until GNSS client is working.
	wg.Add(1)
	go run(videoSender,
		[]string{"--session_id", sessionId, "--config", configFilename})

	wg.Wait()
}
