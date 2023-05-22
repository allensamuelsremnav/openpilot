package log

import (
	"fmt"
	"log"
	"os"
	"path/filepath"
	"sync"
	"time"
)

type Loggable interface {
	String() string
	Timestamp() (int64, error)
}

const binTimestampFmt = "20060102T1504Z"

// Msgs channel should have JSON messages with a time in μs since the
// Unix epoch.  Run as go routine
func Binned(msgs <-chan Loggable, logDir string, wg *sync.WaitGroup) {
	defer wg.Done()

	log.Printf("%s: log dir %s\n", os.Args[0], logDir)
	var oTimestamp string
	var ofile *os.File
	for msg := range msgs {
		ts, err := msg.Timestamp()
		if err != nil {
			log.Fatal(err)
		}

		s := time.UnixMicro(ts).Format(binTimestampFmt)
		if ofile == nil || oTimestamp < s {
			if ofile != nil {
				ofile.Close()
			}
			oTimestamp = s
			fullPath := filepath.Join(logDir, oTimestamp)
			ofile, err = os.Create(fullPath)
			if err != nil {
				log.Fatal(err)
			}
			log.Printf("%s: new log at %s",
				os.Args[0], oTimestamp)
		}
		_, err = fmt.Fprintln(ofile, msg.String())
		if err != nil {
			log.Fatal(err)
		}
	}

	if ofile != nil {
		ofile.Close()
	}
}
