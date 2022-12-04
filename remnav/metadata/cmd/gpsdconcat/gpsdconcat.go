// Concatenate gpsd files relevant to metadata sender_timestamp
package main

import (
	"encoding/csv"
	"flag"
	"io"
	"log"
	"os"
	"path/filepath"
	"sort"
	"strconv"
	"strings"
	"time"

	"remnav.com/remnav/metadata/gpsd"
	storage "remnav.com/remnav/metadata/storage"
)

var verbose bool

func rawInventory(rawDir string) []string {
	// Find all the gpsd log files in rawDir.
	files, err := os.ReadDir(rawDir)
	if err != nil {
		log.Fatal(err)
	}

	suffix := "_g000.json"
	var ret []string
	for _, file := range files {
		if !file.IsDir() {
			n := file.Name()
			nlen := len(n)
			if n[nlen-len(suffix):] == suffix {
				ret = append(ret, n)
			} else if n == "filetransfer.sh" {
				continue
			} else {
				log.Fatalf("unexpected file %s in %s",
					n, rawDir)
			}
		}
	}
	sort.Strings(ret)
	return ret
}

var senderTimestamp = "sender_timestamp"

func metadataSendRange(rdr io.Reader, tag string) (int, int) {
	// Find the [first, last] video send times in ms.
	senderScale := 512
	reader := csv.NewReader(rdr)
	first := -1
	last := -1
	for {
		record, err := reader.Read()
		if err == io.EOF {
			break
		}
		if err, ok := err.(*csv.ParseError); ok && err.Err == csv.ErrFieldCount {
			log.Printf("ignoring %v", err)
			continue
		}
		if err != nil {
			log.Fatalf("%s in %s", err, tag)
		}
		if len(record) < 2 {
			log.Fatalf("%d fields in %s, need at least 2", len(record), record)
		}
		raw := strings.TrimSpace(record[1])
		if raw == senderTimestamp {
			continue
		}
		traw, err := strconv.Atoi(raw)
		if err != nil {
			log.Fatalf("unable to convert '%s' to int in file %s",
				raw, tag)
		}
		t := traw / senderScale
		// sender timestamp might be out of order.
		if first < 0 || t < first {
			first = t
		}
		if last < t {
			last = t
		}
	}
	if verbose {
		log.Printf("[%d, %d] ms %s", first, last, tag)
	}
	return first, last
}

func destinationInventory(destinationDir string) (int, int) {
	// Find the [first, last] sender times in the metadata in destinationDir.
	files, err := os.ReadDir(destinationDir)
	if err != nil {
		log.Fatal(err)
	}

	first := -1
	last := -1
	suffix := ".csv"
	for _, file := range files {
		if !file.IsDir() {
			n := file.Name()
			nlen := len(n)
			if n[nlen-len(suffix):] == suffix {
				f, err := os.Open(filepath.Join(destinationDir, n))
				if err != nil {
					log.Fatal(err)
				}
				defer f.Close()
				s, e := metadataSendRange(f, n)
				if s < 0 {
					continue
				}
				if first < 0 || s < first {
					first = s
				}
				if last < 0 || last < e {
					last = e
				}
			}
		}
	}

	return first, last
}

func main() {
	rawFlag := flag.String("raw",
		"",
		"archive storage directory for raw gpsd logs")
	verboseFlag := flag.Bool("verbose",
		false,
		"verbose output")
	flag.Parse()

	verbose = *verboseFlag

	if false && len(*rawFlag) == 0 {
		log.Fatal("--raw required")
	}
	log.Printf("raw gpsd logs from %s", *rawFlag)

	if flag.NArg() == 0 {
		log.Fatalln("session identifiers required")
	}

	raw := rawInventory(*rawFlag)
	log.Printf("%d raw gpsd logs, [%s, %s]", len(raw), raw[0], raw[len(raw)-1])
	// debugging code:
	// raw := []string{"/home/greg/rn1/remnav/metadata/gpsd/nonl.json",
	//		"/home/greg/rn1/remnav/metadata/gpsd/gpsd.rn3"}
	// gpsd.Concat(raw, os.Stdout)
	// return

	for _, sessionId := range flag.Args() {
		// Process each requested session.
		if verbose {
			log.Printf("session %s", sessionId)
		}

		first, last := destinationInventory(filepath.Join(sessionId, storage.VideoSubdir))
		if verbose {
			log.Printf("[%d, %d] ms %s", first, last, sessionId)
		}
		if first < 0 {
			continue
		}
		firstTime := time.UnixMilli(int64(first)).UTC()
		lastTime := time.UnixMilli(int64(last)).UTC()
		if verbose {
			log.Printf("[%s, %s] %s", firstTime, lastTime, sessionId)
		}

		logSources := gpsd.Intersection(raw, firstTime, lastTime, verbose)
		log.Printf("%d/%d relevant raw gpsd logs found for %s", len(logSources), len(raw),
			sessionId)
		if len(logSources) == 0 {
			continue
		}

		if verbose {
			log.Printf("concatenate %v", logSources)
		}

		outDir := filepath.Join(sessionId, storage.GNSSSubdir)
		err := os.MkdirAll(outDir, 0775)
		if err != nil {
			log.Fatalf("%s: %s while creating directory %s",
				os.Args[0], err, outDir)
		}
		// umask modifies the group permission; fix it.
		err = os.Chmod(outDir, 0775)
		if err != nil {
			log.Fatal(err)
		}
		outlogPath := filepath.Join(outDir, logSources[0])
		outFile, err := os.Create(outlogPath)
		if err != nil {
			log.Fatal(err)
		}
		defer outFile.Close()

		bytesWritten := gpsd.Concat(logSources, outFile)
		log.Printf("%d bytes, %s", bytesWritten, outlogPath)
	}

}
