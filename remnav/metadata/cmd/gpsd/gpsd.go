// Simple check for gpsd watch.
package main

import (
	"flag"
	"log"
	"os"

	gpsd "remnav.com/remnav/metadata/gpsd"
)

func main() {
	GPSDAddrFlag := flag.String("gpsd_addr", "localhost:2947", "address of gpsd, e.g. 10.1.10.225:2947")
	outputFlag := flag.String("o", "", "output file (stdout default)")
	flag.Parse()

	var err error

	oFile := os.Stdout
	if len(*outputFlag) != 0 {
		oFile, err = os.Create(*outputFlag)
		if err != nil {
			log.Fatal(err)
		}
	}

	conn, reader := gpsd.Conn(*GPSDAddrFlag)
	defer conn.Close()

	gpsd.PokeWatch(conn)

	gpsd.Watch(*GPSDAddrFlag, reader, oFile)

}
