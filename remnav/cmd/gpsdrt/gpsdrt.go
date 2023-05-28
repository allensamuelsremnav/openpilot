// Realtime GNSS client.
package main

import (
	"encoding/json"
	"flag"
	"io"
	"log"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	gpsd "remnav.com/remnav/metadata/gpsd"
	"remnav.com/remnav/metadata/storage"
	rnnet "remnav.com/remnav/net"
)

func watch(gpsdAddr string, verbose bool) chan []byte {
	// Watch the gpsd at gpsdAddr and send TPV messages to output chan.
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
				if verbose {
					var tpv gpsd.TPV
					err = json.Unmarshal([]byte(line), &tpv)
					// Trim to part of TVP message.
					log.Printf("gpsdrt: Device %v, Mode %v, Time %s, Lat %.9f°, Lon %.9f°, Speed %.3f m/s",
						tpv.Device, tpv.Mode, tpv.Time.Format(gpsd.RFC339MilliNatural),
						tpv.Lat, tpv.Lon, tpv.Speed)
				}
			} else if deviceCheck && probe.Class == "DEVICES" {
				gpsd.DeviceCheck(gpsdAddr, line)
				deviceCheck = false
			}
		}
	}()
	return msgs
}

func main() {
	gpsdAddress := flag.String("gpsd_addr", "10.0.0.11:2947", "gpsd server host:port, e.g. 10.1.10.225:2947")
	dest := flag.String("dest",
		"10.0.0.60:"+strconv.Itoa(rnnet.OperatorGpsdListen),
		"destination host:port, e.g. 96.64.247.70:"+strconv.Itoa(rnnet.OperatorGpsdListen))
	devs := flag.String("devices", "eth0,eth0",
		"comma-separated list of network devices, e.g. wlan0_1,wlan1_1,wlan2_1")
	heartbeatInterval := flag.Int("heartbeat", 60000, "heartbeat interval ms")
	vehicleRoot := flag.String("vehicle_root",
		"/home/greg/remnav_log",
		"vehicle storage directory, e.g. '/home/user/6TB/vehicle/remconnect'")
	archiveServer := flag.String("archive_server",
		"96.64.247.70",
		"IP address of archive server (e.g. rn3)")
	archiveRoot := flag.String("archive_root",
		"/home/user/6TB/remconnect/archive",
		"archive storage directory (e.g. on rn3)")
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

	var tick <-chan time.Time
	heartbeatDuration := time.Duration(*heartbeatInterval) * time.Millisecond
	heartbeatTicker := time.NewTicker(heartbeatDuration)
	tick = heartbeatTicker.C
	gnssDir := gpsd.LogDir("gpsdrt", *vehicleRoot, storage.RawGNSSSubdir, *archiveServer, *archiveRoot)
	// Get message stream from gpsd server.
	msgs := watch(*gpsdAddress, *verbose)

	// Send msgs to log and via devices to destination.
	logCh := make(chan string)
	udpCh := make(chan []byte)
	go func() {
		if tick != nil {
			defer heartbeatTicker.Stop()
		}
		defer close(udpCh)
		defer close(logCh)

		var heartbeat gpsd.Class
		heartbeat.Class = gpsd.ClassHeartbeat
		for {
			select {
			case msg, ok := <-msgs:
				if !ok {
					return
				}
				udpCh <- msg
				logCh <- string(msg)
			case <-tick:
				heartbeat.Time = time.Now().UTC()
				hb, _ := json.Marshal(heartbeat)
				udpCh <- hb
			}
		}
	}()

	var wg sync.WaitGroup
	wg.Add(1)
	go gpsd.WatchBinned(*gpsdAddress, logCh, gnssDir, &wg)
	rnnet.UDPDup(udpCh, devices, *dest, &wg, false)
	wg.Wait()

}
