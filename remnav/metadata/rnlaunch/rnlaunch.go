package main

import (
	"encoding/json"
	"flag"
	"io/ioutil"
	"log"
	"os"
	experiment "remnav/metadata/experiment"

	"github.com/google/uuid"
)

func main() {
	sessionIdFlag := flag.String("session_id", "", "session id")
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
		log.Println(config.Description)
	}

	// Look for executables
	videoSender := config.Video.VideoSender
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
}
