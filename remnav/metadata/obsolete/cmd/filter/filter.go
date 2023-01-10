// Apply filter to all session below dedup_root.
package main

import (
	"flag"
	"fmt"
	"log"
	"math/rand"
	"os"
	"path"
	"path/filepath"
	"sort"
	"sync"
	"time"

	"github.com/google/uuid"
)

func mocks(dedup_root string, nMocks int) {
	// Generate empty mocks to test file traversal.
	timestamp := time.Now().UTC()
	for i := 0; i < nMocks; i++ {
		sessionId := fmt.Sprintf("%s_%s",
			timestamp.Format("20060102T150405Z"),
			uuid.NewString())
		for _, ext := range []string{"csv", "dat"} {
			_, err := os.Create(filepath.Join(dedup_root, fmt.Sprintf("%s.%s", sessionId, ext)))
			if err != nil {
				log.Fatal(err)
			}
		}
		// Advance the time string by about 30 minutes on average.
		delta := time.Duration(rand.Intn(60 * 1000000000 * 60))
		timestamp = timestamp.Add(delta)
	}
}

// Point of interest according filter.
type filterPoint struct {
	packetNumber int
}

// All points for a session.
type sessionPoints struct {
	sessionId string
	points    []filterPoint
}

var wg sync.WaitGroup

func filterStub(dedupRoot, sessionId, metadataFilename string, points chan<- sessionPoints) {
	// Stub to test file traversal and concurrency.
	log.Print(sessionId)
	points <- sessionPoints{sessionId: sessionId,
		points: nil}
	time.Sleep(time.Duration(rand.Intn(5)) * time.Second)
	log.Println(sessionId, "done")
	wg.Done()
}

func main() {
	dedupRoot := flag.String("dedup_root", "", "root directory for deduped files")
	mock := flag.Int("mocks", 0, "generate mock inputs")
	stub := flag.Bool("stub", false, "use stub filter")
	flag.Parse()
	if len(*dedupRoot) == 0 {
		log.Fatalln("--dedup_root is required")
	}
	log.Println("dedup_root", *dedupRoot)
	if *mock > 0 {
		mocks(*dedupRoot, *mock)
		return
	}

	// TODO plug in real filter operation
	filter := filterStub
	if *stub {
		filter = filterStub
	}

	files, err := os.ReadDir(*dedupRoot)
	if err != nil {
		log.Fatal(err)
	}

	allSessionPoints := make(chan sessionPoints)
	fileCount := 0
	for _, file := range files {
		if file.IsDir() {
			continue
		}
		ext := path.Ext(file.Name())
		if ext != ".csv" {
			continue
		}
		sessionId := file.Name()
		wg.Add(1)
		go filter(*dedupRoot, sessionId, file.Name(),
			allSessionPoints)
		fileCount += 1
	}
	go func() {
		wg.Wait()
		close(allSessionPoints)
	}()

	// Collect all sessions
	var sessions []sessionPoints
	for s := range allSessionPoints {
		sessions = append(sessions, s)
	}
	log.Printf("file count %d, session count %d",
		fileCount, len(sessions))
	// Sort by time (lexicographically).
	sort.Slice(sessions,
		func(i, j int) bool { return sessions[i].sessionId < sessions[j].sessionId })
	for _, s := range sessions {
		fmt.Println(s.sessionId)
	}
	// TODO write csv file
}
