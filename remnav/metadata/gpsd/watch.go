package gpsd

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"path/filepath"
)

func Conn(gpsdAddress string) (net.Conn, *bufio.Reader) {
	// Make connection and reader.
	log.Printf("%s: connecting to gpsd at %s", os.Args[0], gpsdAddress)
	conn, err := net.Dial("tcp4", gpsdAddress)
	if err != nil {
		log.Fatal(err)
	}

	reader := bufio.NewReader(conn)
	line, err := reader.ReadString('\n')
	if err != nil {
		log.Fatal(err)
	}
	// This should be the gpsd version information.
	log.Print(line)
	return conn, reader
}

func PokeWatch(conn net.Conn) {
	// Poke gpsd with a watch request
	param, _ := json.Marshal(
		map[string]interface{}{
			"class":  "WATCH",
			"enable": true,
			"json":   true,
		})
	_, err := fmt.Fprintf(conn, "?WATCH=%s", param)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("?WATCH=%s", param)

}

func DeviceCheck(gpsdAddress, line string) {
	// Misconfiguration could point to a gpsd server with no devices.
	var devices Devices
	err := json.Unmarshal([]byte(line), &devices)
	if err != nil {
		log.Fatal(err)
	}
	log.Printf("%s: %s: ", os.Args[0], devices)
	if len(devices.Devices) == 0 {
		log.Fatalf("no devices found for gpsd at %s.  Is a GPS attached?",
			gpsdAddress)
	}
}

var watchTimestampFmt = "20060102T1504Z"

func logfilename(gnssDir, binTimestamp string) string {
	fmtId := "g000"
	return filepath.Join(gnssDir, binTimestamp+"_"+fmtId+".json")
}

func WatchBinned(gpsdAddress string, reader chan string, gnssDir string) {
	// Gather all of the input from the watch reader; send to a succession of files.
	//
	// Binning rules:
	// * Every TPV message is assigned to a one-minute bin by its time.
	// * Log files are named by the bin that they contain.
	// * Lexicogrphic ordering of log-file names implies temporal ordering
	// of TPV messages.

	deviceCheck := true
	lineCount := 0
	var pending []string
	var otimestamp string
	var ofile *os.File
	for {
		var err error
		line := <-reader
		if err == nil {
			var probe Class
			err := json.Unmarshal([]byte(line), &probe)
			if err != nil {
				log.Fatal(err)
			}
			if deviceCheck {
				if probe.Class == "DEVICES" {
					DeviceCheck(gpsdAddress, line)
					deviceCheck = false
				}
			}

			if ofile == nil {
				if probe.Class == "TPV" {
					// Start a new log file.
					var tpv TPV
					err := json.Unmarshal([]byte(line), &tpv)
					if err != nil {
						log.Fatal(err)
					}
					otimestamp = tpv.Time.Format(watchTimestampFmt)
					ofile, err = os.Create(logfilename(gnssDir, otimestamp))
					if err != nil {
						log.Fatal(err)
					}
					log.Printf("%s: new log at %s",
						os.Args[0], otimestamp)

					// flush pending
					for _, l := range pending {
						_, err = fmt.Fprint(ofile, l)
						if err != nil {
							log.Fatal(err)
						}
					}
					pending = nil

					_, err = fmt.Fprint(ofile, line)
					if err != nil {
						log.Fatal(err)
					}
				} else {
					pending = append(pending, line)
				}
				lineCount++
				continue
			}
			if probe.Class == "TPV" {
				var tpv TPV
				err := json.Unmarshal([]byte(line), &tpv)
				if err != nil {
					log.Fatal(err)
				}
				ts := tpv.Time.Format(watchTimestampFmt)
				if otimestamp != ts {
					// New minute, so new log file
					ofile.Close()
					otimestamp = ts
					var err error
					ofile, err = os.Create(logfilename(gnssDir, otimestamp))
					if err != nil {
						log.Fatal(err)
					}
					log.Printf("%s: new log at %s",
						os.Args[0], otimestamp)
				}
			}
			_, err = fmt.Fprint(ofile, line)
			if err != nil {
				log.Fatal(err)
			}
			lineCount++
		} else if err == io.EOF {
			break
		} else {
			log.Fatal(err)
		}
	}

	log.Printf("%s: %d messages", os.Args[0], lineCount)
	if ofile != nil {
		ofile.Close()
	}
}

func Watch(gpsdAddress string, reader *bufio.Reader, GNSSFile *os.File) {
	// Gather all of the input from the watch reader; send to GNSSFile.
	deviceCheck := true
	lineCount := 0
	for {
		line, err := reader.ReadString('\n')
		if err == nil {
			if deviceCheck {
				// Misconfiguration could point to a gpsd server with no devices.
				var probe Class
				err := json.Unmarshal([]byte(line), &probe)
				if err != nil {
					log.Fatal(err)
				}
				if probe.Class == "DEVICES" {
					var devices Devices
					err := json.Unmarshal([]byte(line), &devices)
					if err != nil {
						log.Fatal(err)
					}
					log.Println(devices)
					if len(devices.Devices) == 0 {
						log.Fatalf("no devices found for gpsd at %s.  Is a GPS attached?",
							gpsdAddress)
					}
					deviceCheck = false
				}
			}
			_, err = fmt.Fprint(GNSSFile, line)
			if err != nil {
				log.Fatal(err)
			}
			lineCount++
			if lineCount%500 == 0 {
				log.Printf("%s: %d messages", os.Args[0], lineCount)
			}
		} else if err == io.EOF {
			break
		} else {
			log.Fatal(err)
		}
	}
}
