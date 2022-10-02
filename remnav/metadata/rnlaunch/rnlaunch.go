package main

import (
	"encoding/json"
	"flag"
	"github.com/google/uuid"
	"io/ioutil"
	"log"
	"os"
	experiment "remnav/metadata/experiment"
)

func main() {
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

	// Look for executables
	videoSender := config.Video.VideoSender
	log.Println("video_sender", videoSender)

	GNSSClient := config.GNSS.GNSSClient
	log.Println("gnss_client", GNSSClient)

	UUID := uuid.NewString()
	log.Println("UUID", UUID)
}
