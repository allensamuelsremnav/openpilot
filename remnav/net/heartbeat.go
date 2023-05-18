package net

import (
	"encoding/json"
	"fmt"
	"log"
	"net"
	"time"
)

// Minimal class for heartbeats, i.e. when BidiWR is used only for operator-->vehicle

const ClassHeartbeat = "HEARTBEAT"

type Heartbeat struct {
	Class string `json:"class"`
}

// Send heartbeats to pc.
func HeartbeatSource(interval time.Duration, pc net.Conn, progress bool) {
	var hb Heartbeat
	hb.Class = ClassHeartbeat
	beat, _ := json.Marshal(hb)
	go func() {
		for {
			_, err := pc.Write(beat)
			if err != nil {
				log.Fatal(err)
			}
			if progress {
				fmt.Printf("h")
			}
			time.Sleep(interval)
		}
	}()

}

// Discard messages on channel.
func HeartbeatSink(recvd <-chan []byte, reportInterval time.Duration) {
	ticker := time.NewTicker(reportInterval)
	defer ticker.Stop()
	var n int
	for {
		select {
		case <-recvd:
			n += 1
		case <-ticker.C:
			log.Printf("%.0f heartbeats/s",
				float64(n)/reportInterval.Seconds())
			n = 0
		}
	}
}
