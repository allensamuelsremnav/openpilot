package gpsd

import (
	"io"
	"log"
	"os"
	"sort"
	"time"
)

func Intersection(raw []string, first, last time.Time, verbose bool) []string {
	// Use the bin rules (WatchBinned) to Find the gpsd logs that
	// intersect the interval [first, last]

	// Filenames of binned logs containing first and last
	firstProbe := logfilename("", first.Format(watchTimestampFmt))
	lastProbe := logfilename("", last.Format(watchTimestampFmt))
	if verbose {
		log.Printf("[%s, %s] probes", firstProbe, lastProbe)
	}

	var ret []string
	for _, s := range raw {
		if firstProbe <= s && s <= lastProbe {
			ret = append(ret, s)
		}
	}
	return ret
}

func Concat(logs []string, destination io.Writer) int {
	// Concatenate log files to destination.  This is a bit more
	// than a cat operation:

	// Assumes that lexicographic ordering of the file names
	// implies temporal ordering of TPV contents.

	// it tries to fix missing newlines at
	// the end of a log file since gpsd log reading expects a
	// newline at the end of each line of JSON.
	newline := []byte{0xA}
	bytesWritten := 0

	sort.Strings(logs)
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
