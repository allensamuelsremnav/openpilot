// Realtime GNSS client.
package main

import (
	"bufio"
	"encoding/json"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"sync"
	"time"

	gpsd "remnav.com/remnav/metadata/gpsd"
)

type latestStr struct {
	mu     sync.RWMutex
	latest string
}

func (l *latestStr) set(s string) {
	l.mu.Lock()
	l.latest = s
	l.mu.Unlock()
}

func (l *latestStr) get() string {
	l.mu.Lock()
	defer l.mu.Unlock()
	return l.latest
}

func watch(conn net.Conn, reader *bufio.Reader,
	latest *latestStr) {
	// Watch the gpsd at conn and remember latest TPV.
	gpsd.PokeWatch(conn)

	// Gather all of the input
	for {
		line, err := reader.ReadString('\n')
		if err == nil {
			var probe gpsd.Class
			err := json.Unmarshal([]byte(line), &probe)
			if err != nil {
				log.Fatal(err)
			}
			if probe.Class == "TPV" {
				var tpv gpsd.TPV
				err := json.Unmarshal([]byte(line), &tpv)
				if err != nil {
					log.Fatal(err)
				}

				if tpv.Mode == gpsd.Mode2D || tpv.Mode == gpsd.Mode3D {
					msg, err := json.Marshal(
						map[string]interface{}{
							"utc":   tpv.Time,
							"speed": tpv.Speed,
						})
					if err != nil {
						log.Fatal(err)
					}
					latest.set(string(msg))
				}
			}
		} else if err == io.EOF {
			break
		} else {
			log.Fatal(err)
		}
	}
}

func main() {
	gpsdAddress := flag.String("gpsd_addr", "", "host:port for gpsd, e.g. localhost:2947")
	serviceAddress := flag.String("service_addr", "", "address for service")
	flag.Parse()

	if len(*gpsdAddress) == 0 {
		log.Fatalf("%s: --gpsd_addr required", os.Args[0])
	}
	if len(*serviceAddress) == 0 {
		log.Fatalf("%s: --service_addr required", os.Args[0])
	}

	// Make connection for service
	serviceConn, err := net.Dial("tcp4", *serviceAddress)
	if err != nil {
		log.Fatal(err)
	}
	defer serviceConn.Close()

	// Make connection and reader for gpsd
	gpsdConn, err := net.Dial("tcp4", *gpsdAddress)
	if err != nil {
		log.Fatal(err)
	}
	defer gpsdConn.Close()

	reader := bufio.NewReader(gpsdConn)
	line, err := reader.ReadString('\n')
	if err != nil {
		log.Fatal(err)
	}
	log.Println(line)

	// The latest TPV data.
	var latest latestStr

	go watch(gpsdConn, reader, &latest)

	for {
		if len(latest.get()) == 0 {
			continue
		}
		log.Print(latest.get())
		fmt.Fprint(serviceConn, latest.get())
		time.Sleep(time.Second)
	}
}
