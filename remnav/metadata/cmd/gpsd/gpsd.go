// Check chedk of gpsd watch.
package main

import (
	"bufio"
	"flag"
	"log"
	"net"
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

	// Make connection and reader.
	log.Printf("%s: connecting to %s", os.Args[0], *GPSDAddrFlag)
	conn, err := net.Dial("tcp4", *GPSDAddrFlag)
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()
	reader := bufio.NewReader(conn)
	line, err := reader.ReadString('\n')
	if err != nil {
		log.Fatal(err)
	}
	// This should be the gpsd version information.
	log.Println(line)

	gpsd.PokeWatch(conn)

	gpsd.Watch(*GPSDAddrFlag, reader, oFile)

}
