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
	"sync"
	"text/template"

	_ "github.com/mattn/go-sqlite3"
	"remnav.com/remnav/metadata/storage"
)

// Matching packets and metadata file.
type channelPair struct {
	packetsFile  string
	metadataFile string
}

// The channel pairs for a dedup operation.
type channelPairs struct {
	sessionId string
	pairs     []channelPair
}

// Return value for a dedup operation.
type dedupReturn struct {
	sessionId string
	err       error
}

var wg sync.WaitGroup

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

func sessionPairs(db *sql.DB, sessionId string) channelPairs {
	// Query for a session's packet and metadata pairs.
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

	var pairs []channelPair
	for rows.Next() {
		var channel, packetFilename, metadataFilename string
		if err := rows.Scan(&channel, &packetFilename, &metadataFilename); err != nil {
			log.Fatal(err)
		}
		pairs = append(pairs,
			channelPair{packetsFile: packetFilename,
				metadataFile: metadataFilename})
	}
	return channelPairs{sessionId: sessionId, pairs: pairs}
}

func dedup(prog, archive_root, dedup_root string, jobs <-chan channelPairs, errs chan<- dedupReturn) {
	// Execute dedup jobs from channel.
	for job := range jobs {
		args := []string{
			"--m", filepath.Join(dedup_root, fmt.Sprintf("%s.csv", job.sessionId)),
			"--p", filepath.Join(dedup_root, fmt.Sprintf("%s.dat", job.sessionId))}
		for _, pair := range job.pairs {
			root_ := filepath.Join(archive_root, job.sessionId, storage.VideoSubdir)
			args = append(args, filepath.Join(root_, pair.metadataFile))
			args = append(args, filepath.Join(root_, pair.packetsFile))
		}
		// log.Printf("%s %s (%d channel pairs)", prog, args, len(job.pairs))
		cmd := exec.Command(prog, args...)
		cmd.Stdout = os.Stdout
		cmd.Stderr = os.Stderr

		err := cmd.Start()
		if err != nil {
			errs <- dedupReturn{sessionId: job.sessionId, err: err}
			continue
		}
		err = cmd.Wait()
		if err != nil {
			errs <- dedupReturn{sessionId: job.sessionId, err: err}
			continue
		}
	}
	wg.Done()
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
	numProc := flag.Int("proc", 4, "number of dedup subprocesses")
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
	log.Println("proc", *numProc)
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
	var allChannelPairs []channelPairs
	for _, session := range allSessions {
		info := sessionPairs(db, session)
		if len(info.pairs) == 0 {
			log.Printf("%s, %d channel pairs, skipping\n", info.sessionId, len(info.pairs))
		}
		allChannelPairs = append(allChannelPairs, info)
	}
	log.Println(len(allChannelPairs), "dedup jobs")

	// Start the workers.
	jobs := make(chan channelPairs)
	errs := make(chan dedupReturn)
	for i := 0; i < *numProc; i++ {
		wg.Add(1)
		go dedup(*dedupProg, *archiveRoot, *dedupRoot, jobs, errs)
	}

	// Send the jobs.
	go func() {
		for _, job := range allChannelPairs {
			jobs <- job
		}
		close(jobs)
	}()

	go func() {
		wg.Wait()
		close(errs)
	}()

	// Read the errors.
	errCount := 0
	for err := range errs {
		if err.err != nil {
			log.Printf("sessionId %s, error %v\n", err.sessionId, err.err)
			errCount += 1
		}
	}

	if errCount > 0 {
		log.Printf("%d / %d jobs with non-zero exit status", errCount, len(allChannelPairs))
		os.Exit(1)
	}
}
