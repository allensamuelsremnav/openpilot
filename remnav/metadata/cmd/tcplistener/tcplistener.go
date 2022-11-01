// Application to debug TCP applications by listening on a port.
package main

import (
	"bufio"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
)

func main() {
	address := flag.String("addr", "", "address for TCP")
	flag.Parse()

	// Make connection for service
	conn, err := net.Dial("tcp4", *address)
	if err != nil {
		log.Fatal(err)
	}
	defer conn.Close()
	reader := bufio.NewReader(conn)

	for {
		line, err := reader.ReadString('\n')
		if err == nil {
			fmt.Print(line)
		} else if err == io.EOF {
			break
		} else {
			log.Fatal(err)
		}
	}
}
