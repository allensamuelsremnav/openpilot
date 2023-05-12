package log

import (
	"encoding/json"
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type logable struct {
	Log int64
}

const binTimestampFmt = "20060102T1504Z"

// Msgs channel should have JSON messages with a time in μs since the
// Unix epoch.  Run as go routine
func Binned(msgs <-chan []byte, logDir string, wg *sync.WaitGroup) {
	defer wg.Done()

	log.Printf("%s: log dir %s\n", os.Args[0], logDir)
	var oTimestamp string
	var ofile *os.File
	for msg := range msgs {
		var probe logable
		err := json.Unmarshal(msg, &probe)
		if err != nil {
			log.Fatal(err)
		}

		ts := time.UnixMicro(probe.Log).Format(binTimestampFmt)
		if ofile == nil || oTimestamp != ts {
			if ofile != nil {
				ofile.Close()
			}
			oTimestamp = ts
			fullPath := filepath.Join(logDir, oTimestamp)
			ofile, err = os.Create(fullPath)
			if err != nil {
				log.Fatal(err)
			}
			log.Printf("%s: new log at %s",
				os.Args[0], oTimestamp)
		}
		_, err = fmt.Fprintln(ofile, string(msg))
		if err != nil {
			log.Fatal(err)
		}
	}

	if ofile != nil {
		ofile.Close()
	}
}
