package gpsd

import (
	"bufio"
	"bytes"
	"encoding/json"
	"fmt"
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
	// than a cat operation because of the newline check.

	// Assumes that lexicographic ordering of the file names
	// implies temporal ordering of TPV contents.

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

		if false {
			tmp := make([]byte, len(logBytes))
			copy(tmp, logBytes)
			buf := bufio.NewReader(bytes.NewBuffer(tmp))
			if err := ParseCheck(buf); err != nil {
				log.Fatal(err)
			}
		}
		if len(logBytes) > 0 && logBytes[len(logBytes)-1] != 0xA {
			// A missing newline at the end of the file is
			// unexpected and may cause trouble parsing
			// the concatenated file.
			log.Printf(
				"skipping %s: missing newline.  This is expected only in TestNewline.",
				l)
			continue
		}

		n, err := destination.Write(logBytes)
		if err != nil {
			log.Fatal(err)
		}
		bytesWritten += n
	}
	return bytesWritten
}

func ParseCheck(rdr *bufio.Reader) error {
	// Verify that we can parse rdr as a gpsd log and that its TPV times are non-decreasing.
	utc := time.Date(2000, 1, 1, 0, 0, 0, 0, time.UTC)
	linecount := 0
	probe := func(line string) error {
		var probe TPV
		err := json.Unmarshal([]byte(line), &probe)
		if err != nil {
			return err
		}
		if probe.Class == "TPV" {
			if probe.Time.Before(utc) {
				return fmt.Errorf("out-of-order timestamp %s at %d",
					probe.Time,
					linecount)
			}
			utc = probe.Time
		}
		return nil
	}
	for {
		line, err := rdr.ReadString('\n')
		if err == nil {
			err := probe(line)
			if err != nil {
				return err
			}
		} else if err == io.EOF {
			if len(line) > 0 {
				err := probe(line)
				if err != nil {
					return err
				}
			}
			break
		} else {
			return err
		}
		linecount += 1
	}
	return nil
}
