package main

import (
	"encoding/json"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"math/rand"
	"os"
	"path/filepath"
	experiment "remnav/metadata/experiment"
	"time"

	"github.com/google/uuid"
)

var cids = []string{"att000", "att001", "tmobile000", "verizon000"}

func video(archiveRoot string, nDirectories int) {
	// mock output files from video receiver
	timestamp := time.Now()
	// mock does not create archive root
	if _, err := os.Stat(archiveRoot); os.IsNotExist(err) {
		log.Fatalln(err)
	}

	for i := 0; i < nDirectories; i++ {
		sessionId := uuid.NewString()
		sessionPath := filepath.Join(
			archiveRoot,
			sessionId)

		videoPath := filepath.Join(sessionPath, "video")
		err := os.MkdirAll(videoPath, 0775)
		if err != nil {
			log.Fatalln(err)
		}
		fmt.Println(videoPath)

		// umask modifies the group permission; fix it
		// so we can also write GNSS data here.
		err = os.Chmod(sessionPath, 0775)
		if err != nil {
			log.Fatalln(err)
		}

		// Make a random number of empty packet and metadata files.
		baseCID := rand.Intn(len(cids))
		nCarriers := rand.Intn(4)
		for j := 0; j < nCarriers; j++ {
			cid := cids[(baseCID+j)%len(cids)]
			timestampStr := timestamp.Format("20060102T150405Z")
			packetFilename := fmt.Sprintf("%s_%s_p000",
				timestampStr, cid)
			metadataFilename := fmt.Sprintf("%s_%s_md099.csv",
				timestampStr,
				cid)
			fmt.Println(packetFilename)
			fmt.Println(metadataFilename)
			_, err := os.Create(filepath.Join(videoPath, packetFilename))
			if err != nil {
				log.Fatalln(err)
			}
			_, err = os.Create(filepath.Join(videoPath, metadataFilename))
			if err != nil {
				log.Fatalln(err)
			}
			// Advance the time string by about 30 minutes on average.
			delta := time.Duration(rand.Intn(60 * 1000000000 * 60))
			timestamp = timestamp.Add(delta)
		}
	}
}

func main() {
	fmt.Println("hello,world")
	sessions := flag.Int("sessions", 10, "number of sessions")
	flag.Parse()
	if len(flag.Args()) == 0 {
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

	log.Printf("configuration description \"%s\"", config.Description)
	log.Println("gnss archive_root", config.GNSS.Storage.ArchiveRoot)
	log.Println("video archive_root", config.Video.Storage.ArchiveRoot)
	log.Println("sessions", *sessions)
	video(config.Video.Storage.ArchiveRoot, *sessions)
}
