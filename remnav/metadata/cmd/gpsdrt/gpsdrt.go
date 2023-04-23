// Realtime GNSS client.
package main

import (
	"encoding/json"
	"flag"
	"io"
	"log"
	"os"
	"strings"

	gpsd "remnav.com/remnav/metadata/gpsd"
	rnnet "remnav.com/remnav/net"
)

func watch(gpsdAddr string) chan []byte {
	// Watch the gpsd at conn and remember latest TPV.
	conn, reader := gpsd.Conn(gpsdAddr)

	gpsd.PokeWatch(conn)

	msgs := make(chan []byte)
	go func() {
		deviceCheck := true
		defer close(msgs)
		// Gather all of the input
		for {
			line, err := reader.ReadString('\n')
			if err != nil {
				if err == io.EOF {
					break
				}
				log.Fatal(err)
			}
			var probe gpsd.Class
			err = json.Unmarshal([]byte(line), &probe)
			if err != nil {
				log.Fatal(err)
			}
			// Clients only need TPV contents.
			if probe.Class == "TPV" {
				msgs <- []byte(line)
			} else if deviceCheck && probe.Class == "DEVICES" {
				gpsd.DeviceCheck(gpsdAddr, line)
				deviceCheck = false
			}
		}
	}()
	return msgs
}

func main() {
	gpsdAddress := flag.String("gpsd_addr", "10.0.0.11:2947", "gpsd server host:port, e.g. 10.0.0.11:2947")
	dest := flag.String("dest", "96.64.247.70:6001", "destination host:port")

	devs := flag.String("devices", "eth0,eth0", "comma-separated list of network devices, e.g. eth0,wlan0")
	verbose := flag.Bool("verbose", false, "verbosity on")

	flag.Parse()

	if len(*gpsdAddress) == 0 {
		log.Fatalf("%s: --gpsd_addr required", os.Args[0])
	}

	var devices []string
	for _, t := range strings.Split(*devs, ",") {
		if len(t) == 0 {
			continue
		}
		devices = append(devices, t)
	}

	// Get message stream from gpsd server.
	msgs := watch(*gpsdAddress)

	// Send via device to destination.
	rnnet.UDPDup(msgs, devices, *dest, *verbose)
}
