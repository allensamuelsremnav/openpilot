package main

import (
	"encoding/json"
	"flag"
	"io/ioutil"
	"log"
	"os"
	"github.com/google/uuid"
	experimentconfig "remnav/metadata/experimentconfig"
)

func main() {
	flag.Parse()
	log.Println("arguments", flag.Args())
	if len(flag.Args()) != 1 {
		log.Fatalln("expected one positional argument, got", flag.Args())
	}
	// Read config file.
	config_filename := flag.Args()[0]
	log.Println("config file", config_filename)
	config_file, err := os.Open(config_filename)
	if err != nil {
		log.Fatalln(err)
	}
	defer config_file.Close()
	byteValue, err := ioutil.ReadAll(config_file)
	if err != nil {
		log.Fatalln(err)
	}

	// Make it JSON.
	var config experimentconfig.Config
	json.Unmarshal([]byte(byteValue), &config)

	// Look for executables
	video_sender := config.Video.VideoSender
	log.Println("video_sender", video_sender)

	gnss_client := config.GNSS.GNSSClient
	log.Println("gnss_client", gnss_client)

	UUID := uuid.NewString()
	log.Println("UUID", UUID)
}
