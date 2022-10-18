package storage

import (
	"encoding/json"
	"fmt"
	"io"
	"log"
	"os"
	"path"
	"path/filepath"
	"strings"

	experiment "remnav.com/remnav/metadata/experiment"
)

type IdDesc struct {
	Id          string `json:"id"`
	Description string `json:"description"`
}

// By convention, the timestamp embedded in the filename is
// YYYYmmddTHHMMSSZ.
type videoFile struct {
	Filename  string
	Timestamp string
	Cellular  string
	Format    string
}

type VideoPacketFile videoFile
type VideoMetadataFile videoFile

type VideoFiles struct {
	PacketFiles   []VideoPacketFile
	MetadataFiles []VideoMetadataFile
}

type GNSSTrackFile struct {
	Filename  string
	Timestamp string
	Format    string
}

type Session struct {
	Id          string
	Source      string
	Destination string
	Description string
	Video       VideoFiles
	GNSS        []GNSSTrackFile
}

// Video files are stored in this subdirectory of the session directory
var VideoSubdir = "video"
var walkerId = "archive_walker"

func readConfigFile(configPath string) *experiment.Config {
	// Read config file if available.
	_, err := os.Stat(configPath)
	if os.IsNotExist(err) {
		return nil
	}

	configFile, err := os.Open(configPath)
	if err != nil {
		log.Fatalln(err)
	}
	defer configFile.Close()
	configBytes, err := io.ReadAll(configFile)
	if err != nil {
		log.Fatalln(err)
	}

	// Interpret as JSON.
	var config experiment.Config
	json.Unmarshal([]byte(configBytes), &config)
	return &config
}

func session(archiveRoot string, sessionFile os.DirEntry) *Session {
	configPath := filepath.Join(archiveRoot, sessionFile.Name(), "experiment.json")
	config := readConfigFile(configPath)
	var source, destination, description string
	if config != nil {
		source = (*config).Video.VideoSource
		destination = (*config).Video.VideoDestination
		description = (*config).Description
		fmt.Printf("%s: %s, %s, \"%s\"\n", sessionFile.Name(), source, destination, description)
	}

	// Make slices for the data files for the directory sessionFile
	// Just the video files for now.
	videoPath := filepath.Join(archiveRoot, sessionFile.Name(), VideoSubdir)
	info, err := os.Stat(videoPath)
	if os.IsNotExist(err) || !info.IsDir() {
		log.Println(walkerId, videoPath, "not found or not a directory, skipping", sessionFile.Name())
		return nil
	}
	files, err := os.ReadDir(videoPath)
	if err != nil {
		log.Fatal(err)
	}

	var packetFiles []VideoPacketFile
	var metadataFiles []VideoMetadataFile

	for _, file := range files {
		if file.IsDir() {
			continue
		}
		ext := path.Ext(file.Name())
		tokens := strings.Split(strings.TrimSuffix(file.Name(), ext), "_")
		if len(tokens) != 3 {
			log.Fatalf("expected timestamp, carrier id, and fmt id, got %v",
				tokens)
		}
		if len(ext) == 0 || ext == ".dat" {
			packetFiles = append(packetFiles,
				VideoPacketFile{Filename: file.Name(),
					Timestamp: tokens[0],
					Cellular:  tokens[1],
					Format:    tokens[2]})
		} else if ext == ".csv" {
			metadataFiles = append(metadataFiles,
				VideoMetadataFile{Filename: file.Name(),
					Timestamp: tokens[0],
					Cellular:  tokens[1],
					Format:    tokens[2]})
		} else {
			log.Println("skipping %s, unrecognized extension %s", file.Name(), ext)
		}
	}
	return &Session{Id: sessionFile.Name(),
		Source:      source,
		Destination: destination,
		Description: description,
		Video: VideoFiles{PacketFiles: packetFiles,
			MetadataFiles: metadataFiles}}

}
func Walker(archiveRoot string) []Session {
	// Compute a slice of Session objects for the data at archiveRoot
	files, err := os.ReadDir(archiveRoot)
	if err != nil {
		log.Fatal(walkerId, err)
	}

	var sessions []Session
	for _, file := range files {
		if file.IsDir() {
			session := session(archiveRoot, file)
			if session == nil {
				continue
			}
			sessions = append(sessions,
				*session)
		}
	}
	return sessions
}
