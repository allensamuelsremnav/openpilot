package main

import (
	"database/sql"
	"flag"
	"fmt"
	"io/ioutil"
	"log"
	"os"

	storage "remnav/metadata/storage"

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
	tmpfile, err := ioutil.TempFile("", "metadata.*.db")
	if err != nil {
		log.Fatal(err)
	}

	// Caution: PRAGMA foreign_keys = ON is not sticky.
	DSN := fmt.Sprintf("file:%s?_foreign_keys=yes", tmpfile.Name())
	log.Println(DSN)
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

func initIds(db *sql.DB, jsonPath string) {

	idInit := storage.InitialIds()

	fmt.Println("video_source", idInit.VideoSource)
	fmt.Println("video_destination", idInit.VideoDestination)
	fmt.Println("cellular", idInit.Cellular)
	fmt.Println("gnss_receiver", idInit.GNSSReceiver)
	fmt.Println(idInit)
}

func main() {
	archiveRoot := flag.String("archive_root", "/home/user/6TB/remconnect", "top-level directory for storage of video data")
	dbFilename := flag.String("database", "./metadata.db", "output Sqlite3 database")
	idsFilename := flag.String("ids_init", "./ids.json", "JSON file with id definitions")

	flag.Parse()

	log.Println(*archiveRoot)
	log.Println(*dbFilename)
	log.Println(*idsFilename)

	sessions := storage.Walker(*archiveRoot)
	for _, s := range sessions {
		log.Printf("session %s", s.Id)
	}

	// Make a working db that we will later mv to the destination database
	working, workingFilename := workdb()

	// Add the known ids
	initIds(working, *idsFilename)

	/*
		insert := "INSERT INTO video_session (id, source, destination) VALUES (\"zzzz\", \"vehicle000\", \"operator000\");"
		if _, err := db.Exec(insert); err != nil {
			log.Fatalf("%v: %s", err, insert)
		}
	*/

	working.Close()

	// This is atomic
	err := os.Rename(workingFilename, *dbFilename)
	if err != nil {
		log.Fatal(err)
	}
	/*
		rows, err := db.Query("SELECT * from VIDEO_SESSION")
		if err != nil {
			log.Fatal(err)
		}
		defer rows.Close()
		for rows.Next() {
			var id string
			var maybeSource, maybeDestination, maybeDescription sql.NullString
			if err := rows.Scan(&id, &maybeSource, &maybeDestination, &maybeDescription); err != nil {
				log.Fatal(err)
			}
			fmt.Println(id, maybeNull(maybeSource), maybeNull(maybeDestination),
				maybeNull(maybeDescription))
		}
	*/
}
