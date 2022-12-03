package gpsd

import (
	"io"
	"log"
	"os"
	"time"
)

func Intersection(raw []string, first, last time.Time, verbose bool) []string {
	// Find the gpsd logs that intersect the interval [first, last]
	if len(raw) == 0 {
		return nil
	}
	// Filenames of raw logs containing first and last
	firstProbe := logfilename("", first.Format(watchTimestampFmt))
	lastProbe := logfilename("", last.Format(watchTimestampFmt))
	if verbose {
		log.Printf("[%s, %s] probes", firstProbe, lastProbe)
	}
	logFirst := -1
	logLast := len(raw)
	// intersect [firstProbe, infinity) with raw logs.
	if raw[len(raw)-1] < firstProbe {
		if verbose {
			log.Printf("%s was after gpsd logs",
				first)
		}
		return nil
	}
	for i, s := range raw {
		if firstProbe <= s {
			logFirst = i
			break
		}
	}

	// intersect (-infinity, lastProbe] with raw logs.
	if lastProbe < raw[0] {
		if verbose {
			log.Printf("%s was before gpsd logs",
				last)
		}
		return nil
	}
	for i := len(raw) - 1; i >= 0; i-- {
		if raw[i] <= lastProbe {
			logLast = i
			break
		}
	}
	if logLast < logFirst {
		if verbose {
			log.Printf("no intersection with logs, logLast %d < logFirst %d",
				logLast, logFirst)
		}
		return nil
	}
	if logFirst == -1 || logLast == len(raw) {
		log.Fatalf("programming error, logFirst %d, logLast %d",
			logFirst, logLast)
	}
	return raw[logFirst : logLast+1]
}

func Concat(logs []string, destination io.Writer) int {
	// Concatenate log files to destination.  This is a bit more
	// than a cat operation: it tries to fix missing newlines at
	// the end of a log file since gpsd log reading expects a
	// newline at the end of each line of JSON.
	newline := []byte{0xA}
	bytesWritten := 0
	for _, l := range logs {
		logfile, err := os.Open(l)
		if err != nil {
			log.Fatal(err)
		}
		defer logfile.Close()
		logBytes, err := io.ReadAll(logfile)
		if err != nil {
			log.Fatal(err)
		}
		n, err := destination.Write(logBytes)
		if err != nil {
			log.Fatal(err)
		}
		bytesWritten += n
		// It's possible that the newline is missing at the end of a gpsd log file.
		if len(logBytes) > 0 && logBytes[len(logBytes)-1] != 0xA {
			destination.Write(newline)
			log.Printf(
				"missing newline in %s; attempting patch. If this not during a regression test, please report to greg.lee@@remnav.com",
				l)
			bytesWritten += len(newline)
		}
	}
	return bytesWritten
}
