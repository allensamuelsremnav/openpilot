// Package main generates a sqlite3 database for the files at the archive root.
package main

import (
	"database/sql"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"

	storage "remnav.com/remnav/metadata/storage"

	_ "github.com/mattn/go-sqlite3"
)

func maybeNull(maybeString sql.NullString) string {
	if maybeString.Valid {
		return maybeString.String
	}
	return "NULL"
}

func workdb() (*sql.DB, string) {
	// Open and initialize a working database.
	// Create a temp file.
	tmpfile, err := ioutil.TempFile("", "metadata.*.db")
	if err != nil {
		log.Fatal(err)
	}
	err = tmpfile.Close()
	if err != nil {
		log.Fatal(err)
	}

	// Caution: PRAGMA foreign_keys = ON is not sticky.
	DSN := fmt.Sprintf("file:%s?_foreign_keys=yes", tmpfile.Name())
	log.Println("DSN: ", DSN)
	db, err := sql.Open("sqlite3", DSN)
	if err != nil {
		log.Fatal(err)
	}

	for _, v := range storage.Schema() {
		if _, err := db.Exec(v); err != nil {
			log.Fatal(err)
		}
	}

	return db, tmpfile.Name()
}

func insertId(db *sql.DB, table string, ids map[string]storage.IdDesc) {
	// Insert IdDesc into table.
	for _, v := range ids {
		insert := fmt.Sprintf("INSERT INTO %s(id, description) VALUES(\"%s\", \"%s\");",
			table, v.Id, v.Description)
		if _, err := db.Exec(insert); err != nil {
			log.Fatalf("%v: %s", err, insert)
		}
	}
}

func makeIdMap(ids []storage.IdDesc) map[string]storage.IdDesc {
	// Make a string -> IdDesc map from a slice of IdDesc
	ret := make(map[string]storage.IdDesc)
	for _, v := range ids {
		ret[v.Id] = v
	}
	return ret
}
func sessionPass1(sessions []storage.Session, db *sql.DB) {
	// First pass: resolve foreign keys
	idInit := storage.InitialIds()
	videoSources := makeIdMap(idInit.VideoSource)
	videoDestinations := makeIdMap(idInit.VideoDestination)

	cellular := makeIdMap(idInit.Cellular)
	// GNSSReceiver := makeIdMap(idInit.GNSSReceiver)
	packetFormats := makeIdMap(idInit.VideoPacketFormat)
	metadataFormats := makeIdMap(idInit.VideoMetadataFormat)
	// GNSSFormats := makeIdMap(idInit.GNSSTrackFormat)

	for _, s := range sessions {
		log.Printf("pass1: session %s, packet files %d, gnss files %d",
			s.Id, len(s.Video.PacketFiles), len(s.GNSS))

		// Find new foreign keys:
		// video_source(id), video_destination(id), cellular(id), gnss_receiver(id) TODO,
		// video_metadata_format(id), video_packets_format(id)
		// gnss_track_format(id) TODO
		if len(s.Source) != 0 {
			_, ok := videoSources[s.Source]
			if !ok {
				videoSources[s.Source] = storage.IdDesc{Id: s.Source}
			}
		}
		if len(s.Destination) != 0 {
			_, ok := videoDestinations[s.Destination]
			if !ok {
				videoDestinations[s.Destination] = storage.IdDesc{Id: s.Destination}
			}
		}
		for _, v := range s.Video.PacketFiles {
			_, ok := cellular[v.Cellular]
			if !ok {
				cellular[v.Cellular] = storage.IdDesc{Id: v.Cellular}
			}
			_, ok = packetFormats[v.Format]
			if !ok {
				packetFormats[v.Format] = storage.IdDesc{Id: v.Format}
			}
		}
		for _, v := range s.Video.MetadataFiles {
			_, ok := cellular[v.Cellular]
			if !ok {
				cellular[v.Cellular] = storage.IdDesc{Id: v.Cellular}
			}
			_, ok = metadataFormats[v.Format]
			if !ok {
				metadataFormats[v.Format] = storage.IdDesc{Id: v.Format}
			}
		}

	}

	insertId(db, "video_source", videoSources)
	insertId(db, "video_destination", videoDestinations)
	insertId(db, "cellular", cellular)
	insertId(db, "video_packets_format", packetFormats)
	insertId(db, "video_metadata_format", metadataFormats)
}

func sessionPass2(sessions []storage.Session, db *sql.DB) {
	// Add the files after foreign-key references have been resolved in pass 1
	for _, s := range sessions {
		var sessionInsert string
		if len(s.Source) > 0 && len(s.Destination) > 0 {
			sessionInsert = fmt.Sprintf(
				"INSERT INTO video_session(id, source, destination, description) VALUES (\"%s\",\"%s\",\"%s\",\"%s\");",
				s.Id, s.Source, s.Destination, s.Description)
		} else {
			// Avoid foreign-key errors.
			sessionInsert = fmt.Sprintf(
				"INSERT INTO video_session(id) VALUES (\"%s\");",
				s.Id)
		}
		if _, err := db.Exec(sessionInsert); err != nil {
			log.Fatalf("%v: %s", err, sessionInsert)
		}

		for _, v := range s.Video.PacketFiles {
			insert := fmt.Sprintf("INSERT INTO video_packets(video_session, filename, start_time, cellular, format) VALUES(\"%s\",\"%s\",\"%s\",\"%s\",\"%s\");",
				s.Id, v.Filename, v.Timestamp, v.Cellular, v.Format)
			if _, err := db.Exec(insert); err != nil {
				log.Fatalf("%v: %s", err, insert)
			}
		}
		for _, v := range s.Video.MetadataFiles {
			insert := fmt.Sprintf("INSERT INTO video_metadata(video_session, filename, start_time, cellular, format) VALUES(\"%s\",\"%s\",\"%s\",\"%s\",\"%s\");",
				s.Id, v.Filename, v.Timestamp, v.Cellular, v.Format)
			if _, err := db.Exec(insert); err != nil {
				log.Fatalf("%v: %s", err, insert)
			}
		}
	}
}

func main() {
	archiveRoot := flag.String("archive_root", "/home/user/6TB/remconnect", "top-level directory for storage of video data")
	dbFilename := flag.String("database", "./metadata.db", "output Sqlite3 database")

	flag.Parse()

	log.Println("archive_root", *archiveRoot)
	log.Println("database", *dbFilename)

	// Read file system once
	sessions := storage.Walker(*archiveRoot)

	// Make a working db that we will later mv to the destination database
	working, workingFilename := workdb()
	log.Println(workingFilename)

	sessionPass1(sessions, working)
	sessionPass2(sessions, working)

	err := working.Close()
	if err != nil {
		log.Fatal(err)
	}

	// This is atomic
	err = os.Rename(workingFilename, *dbFilename)
	if err != nil {
		log.Fatal(err)
	}
}
