package storage

import (
	"io/ioutil"
	"log"
	"os"
	"path"
	"path/filepath"
	"strings"
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

func session(archiveRoot string, sessionFile os.FileInfo) ([]VideoPacketFile, []VideoMetadataFile) {
	// Make slices for the data files for the directory sessionFile
	// Just the video files for now.
	videoPath := filepath.Join(archiveRoot, sessionFile.Name(), VideoSubdir)
	info, err := os.Stat(videoPath)
	if os.IsNotExist(err) || !info.IsDir() {
		log.Println(walkerId, videoPath, "not found or not a directory, skipping", sessionFile.Name())
		return nil, nil
	}
	files, err := ioutil.ReadDir(videoPath)
	if err != nil {
		log.Fatal(err)
	}

	var packetFiles []VideoPacketFile
	var metadataFiles []VideoMetadataFile

	for _, file := range files {
		if file.IsDir() {
			continue
		}
		if len(path.Ext(file.Name())) == 0 {
			tokens := strings.Split(file.Name(), "_")
			if len(tokens) != 3 {
				log.Fatalf("expected timestamp, carrier id, and fmt id, got %v",
					tokens)
			}
			packetFiles = append(packetFiles,
				VideoPacketFile{Filename: file.Name(),
					Timestamp: tokens[0],
					Cellular:  tokens[1],
					Format:    tokens[2]})
		} else {
			ext := path.Ext(file.Name())
			tokens := strings.Split(strings.TrimSuffix(file.Name(), ext), "_")
			if len(tokens) != 3 {
				log.Fatalf("expected timestamp, carrier id, and fmt id, got %v",
					tokens)
			}
			metadataFiles = append(metadataFiles,
				VideoMetadataFile{Filename: file.Name(),
					Timestamp: tokens[0],
					Cellular:  tokens[1],
					Format:    tokens[2]})
		}
	}
	return packetFiles, metadataFiles
}

func Walker(archiveRoot string) []Session {
	// Compute a slice of Session objects for the data at archiveRoot
	files, err := ioutil.ReadDir(archiveRoot)
	if err != nil {
		log.Fatal(walkerId, err)
	}

	var sessions []Session
	for _, file := range files {
		if file.IsDir() {
			packetFiles, metadataFiles := session(archiveRoot, file)
			if packetFiles == nil {
				continue
			}
			sessions = append(sessions,
				Session{Id: file.Name(),
					Video: VideoFiles{PacketFiles: packetFiles,
						MetadataFiles: metadataFiles}})
		}
	}
	return sessions
}
