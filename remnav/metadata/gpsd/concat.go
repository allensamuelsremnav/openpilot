package gpsd

import (
	"io"
	"log"
	"os"
)

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
				"missing newline in %s.  Attempting patch; please report to greg.lee@@remnav.com",
				l)
			bytesWritten += len(newline)
		}
	}
	return bytesWritten
}
