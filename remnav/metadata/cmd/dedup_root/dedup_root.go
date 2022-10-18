// Apply dedup to metadata and packet files below archive_root.
package main

import (
	"bytes"
	"database/sql"
	"flag"
	"fmt"
	"log"
	"os"
	"os/exec"
	"path/filepath"
	"runtime"
	"text/template"

	_ "github.com/mattn/go-sqlite3"
	"remnav.com/remnav/metadata/storage"
)

type ChannelPair struct {
	PacketsFile  string
	MetadataFile string
}

// The channel pairs for a dedup operation.
type ChannelPairs struct {
	SessionId string
	Pairs     []ChannelPair
}

func sessionIds(db *sql.DB, table string) []string {
	// Query table for session ids
	q := `SELECT id from {{.sessionTable}}`
	q_ := template.Must(template.New("").Parse(q))
	var qBuff bytes.Buffer
	q_.Execute(&qBuff,
		map[string]interface{}{
			"sessionTable": table})
	// log.Println(qBuff.String())
	rows, err := db.Query(qBuff.String())
	if err != nil {
		log.Fatal(err)
	}
	var sessions []string
	for rows.Next() {
		var sessionId string
		if err := rows.Scan(&sessionId); err != nil {
			log.Fatal(err)
		}
		sessions = append(sessions, sessionId)
	}
	return sessions
}

func channelPairs(db *sql.DB, sessionId string) ChannelPairs {
	// Query for packet and metadata pairs.
	q := `
SELECT packets.channel, packets.filename, metadata.filename
FROM
  (SELECT cellular as channel, filename
  FROM video_packets
  WHERE video_session = "{{.sessionID}}"
  ) as packets
  JOIN
  (SELECT cellular as channel, filename
  FROM video_metadata
  WHERE video_session = "{{.sessionID}}"
  ) as metadata
  ON packets.channel = metadata.channel
`
	q_ := template.Must(template.New("").Parse(q))
	var qBuff bytes.Buffer
	q_.Execute(&qBuff,
		map[string]interface{}{
			"sessionID": sessionId})
	// log.Println(qBuff.String())
	rows, err := db.Query(qBuff.String())
	if err != nil {
		log.Fatal(err)
	}

	var channelPairs []ChannelPair
	for rows.Next() {
		var channel, packetFilename, metadataFilename string
		if err := rows.Scan(&channel, &packetFilename, &metadataFilename); err != nil {
			log.Fatal(err)
		}
		channelPairs = append(channelPairs,
			ChannelPair{PacketsFile: packetFilename,
				MetadataFile: metadataFilename})
	}
	return ChannelPairs{SessionId: sessionId, Pairs: channelPairs}
}

func dedup(prog, archive_root, dedup_root string, jobs <-chan ChannelPairs, errs chan<- error) {
	// Execute dedup jobs from channel.
	for job := range jobs {
		args := []string{
			"--m", filepath.Join(dedup_root, fmt.Sprintf("%s.csv", job.SessionId)),
			"--p", filepath.Join(dedup_root, fmt.Sprintf("%s.dat", job.SessionId))}
		for _, pair := range job.Pairs {
			root_ := filepath.Join(archive_root, job.SessionId, storage.VideoSubdir)
			args = append(args, filepath.Join(root_, pair.MetadataFile))
			args = append(args, filepath.Join(root_, pair.PacketsFile))
		}
		// log.Printf("%s %s (%d channel pairs)", prog, args, len(job.Pairs))
		cmd := exec.Command(prog, args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr

		// Every iteration must put a return status into errs; otherwise main will hang.
		err := cmd.Start()
		if err != nil {
			errs <- err
			continue
		}
		err = cmd.Wait()
		if err != nil {
			errs <- err
			continue
		}
		errs <- nil
	}
}

func main() {
	// go run . --archive_root ~/remconnect -dedup_root ~/dedup -metadata_db ~/rn1/remnav/metadata/sqlite3/metadata.db --dedup_prog ./dedup.sh
	log.Println("GOMAXPROCS", runtime.GOMAXPROCS(0))
	archiveRoot := flag.String("archive_root", "",
		"root directory for archive storage, e.g. /home/user/6TB/remconnect on rn3")
	metadataDB := flag.String("metadata_db", "./metadata.db", "database with metadata about files at archive_root")
	// Prevent SQL injection
	tableOverride := flag.Bool("table_override", false, "use table \"dedup_override\" instead of video_session")
	dedupProg := flag.String("dedup_prog", "", "dedup executable")
	dedupRoot := flag.String("dedup_root", "", "root directory for deduped files")
	numDedups := flag.Int("num_dedup", 4, "dedup parallelism")
	flag.Parse()
	if len(*archiveRoot) == 0 {
		log.Fatalln("--archive_root is required")
	}
	if len(*dedupRoot) == 0 {
		log.Fatalln("--dedup_root is required")
	}
	if len(*dedupProg) == 0 {
		log.Fatalln("--dedup_prog is required")
	}
	log.Println("archive_root", *archiveRoot)
	log.Println("dedup_root", *dedupRoot)
	log.Println("metadata_db", *metadataDB)
	log.Println("num_dedup", *numDedups)
	dbTable := "video_session"
	if *tableOverride {
		dbTable = "dedup_override"
	}
	log.Println("table", dbTable)

	// Set up database.
	DSN := fmt.Sprintf("file:%s?_foreign_keys=yes", *metadataDB)
	log.Println("DSN: ", DSN)
	db, err := sql.Open("sqlite3", DSN)
	if err != nil {
		log.Fatal(err)
	}

	// Find the channel pairs for all sessions in dbTable
	allSessions := sessionIds(db, dbTable)
	var allChannelPairs []ChannelPairs
	for _, session := range allSessions {
		info := channelPairs(db, session)
		if len(info.Pairs) == 0 {
			log.Printf("%s, %d channel pairs, skipping\n", info.SessionId, len(info.Pairs))
		}
		allChannelPairs = append(allChannelPairs, info)
	}
	log.Println("dedup jobs", len(allChannelPairs))

	// Start the workers.
	jobs := make(chan ChannelPairs, len(allChannelPairs))
	errs := make(chan error, len(allChannelPairs))
	for i := 0; i < *numDedups; i++ {
		go dedup(*dedupProg, *archiveRoot, *dedupRoot, jobs, errs)
	}

	// Send the jobs.
	for _, job := range allChannelPairs {
		jobs <- job
	}
	close(jobs)

	// Check for first error.
	for i := 0; i < len(allChannelPairs); i++ {
		if err := <-errs; err != nil {
			log.Fatal(err)
		}
	}
}
