package net

import (
	"log"
	"net"
)

// Make a chan for the packets and readfrom addr on a PacketConn
func Chan(pc net.PacketConn, bufSize int) (<-chan []byte, chan net.Addr) {
	msgs := make(chan []byte)
	addrs := make(chan net.Addr)
	go func() {
		var prev net.Addr
		for {
			buf := make([]byte, bufSize)
			n, addr, err := pc.ReadFrom(buf)
			if err != nil {
				log.Fatal(err)
			}
			msgs <- buf[:n]
			if prev == nil || addr != prev {
				prev = addr
				addrs <- addr
			}
		}
		close(msgs)
		close(addrs)
	}()
	return msgs, addrs
}

// All we require from Timestamp is that s < t ---> s is before t.
type Key struct {
	Type      string
	Timestamp int64
}

// The returned channel will contain any msg of a given Type whose
// Timestamp is later than the Timestamp of any msg with the same Type
// that was already written to the channel. Equivalently: for each
// Type, out-of-order or redundant messages are dropped.
func Latest(msgs <-chan []byte, get func(msg []byte) Key) <-chan []byte {
	out := make(chan []byte)
	go func() {
		latest := make(map[string]int64)
		for msg := range msgs {
			key := get(msg)
			if key.Timestamp == 0 {
				log.Printf("message %s with zero timestamp",
					key.Type)
				continue
			}
			ts, ok := latest[key.Type]
			if !ok || (ok && key.Timestamp > ts) {
				latest[key.Type] = key.Timestamp
				out <- msg
			}
		}
		close(out)
	}()
	return out
}
