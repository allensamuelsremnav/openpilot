// Offline GNSS_CLIENT.
package main

import (
	"flag"
	"fmt"
	"io"
	"log"
	"os"
	"sync"

	gpsd "remnav.com/remnav/metadata/gpsd"
)

func main() {
	// Listen to gpsd and write messages to
	// <vehicle_root>/gnss/<machine_id>/<YYYYmmddTHHMM_<fmtid>.json>
	// ./gpsdol
	vehicleRootFlag := flag.String("vehicle_root",
		"/home/user/6TB/vehicle/remconnect",
		"vehicle storage directory")
	archiveServerFlag := flag.String("archive_server",
		"96.64.247.70",
		"IP address of archive server (e.g. rn3)")
	archiveRootFlag := flag.String("archive_root",
		"/home/user/6TB/remconnect/archive",
		"archive storage directory (e.g. on rn3)")
	gpsdAddressFlag := flag.String("gpsd_address",
		"10.1.10.225:2947",
		"local address of gpsd server")
	flag.Usage = func() {
		fmt.Fprintf(os.Stderr, "Usage: %s [OPTIONS]\n", os.Args[0])
		flag.PrintDefaults()
	}
	flag.Parse()
	if flag.NArg() > 0 {
		log.Printf("ignoring unexpected argument %s\n", flag.Arg(0))
	}

	gnssPath := gpsd.LogDir("gpsdol", *vehicleRootFlag, *archiveServerFlag, *archiveRootFlag)

	// Set up the connection to gpsd.
	conn, reader := gpsd.Conn(*gpsdAddressFlag)
	defer conn.Close()

	gpsd.PokeWatch(conn)

	msgs := make(chan string)
	go func() {
		for {
			line, err := reader.ReadString('\n')
			if err == nil {
				msgs <- line
			} else if err == io.EOF {
				break
			} else {
				log.Fatal(err)
			}
		}
		close(msgs)
	}()

	var wg sync.WaitGroup
	wg.Add(1)
	gpsd.WatchBinned(*gpsdAddressFlag, msgs, gnssPath, &wg)
	wg.Wait()
}
