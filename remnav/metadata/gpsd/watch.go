package gpsd

import (
	"bufio"
	"encoding/json"
	"fmt"
	"io"
	"log"
	"net"
	"os"
)

func Conn(GPSDAddr string) (net.Conn, *bufio.Reader) {
	// Make connection and reader.
	log.Printf("%s: connecting to %s", os.Args[0], GPSDAddr)
	conn, err := net.Dial("tcp4", GPSDAddr)
	if err != nil {
		log.Fatal(err)
	}

	reader := bufio.NewReader(conn)
	line, err := reader.ReadString('\n')
	if err != nil {
		log.Fatal(err)
	}
	// This should be the gpsd version information.
	log.Println(line)
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
