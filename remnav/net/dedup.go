package net

import (
	"log"
	"net"

	"golang.org/x/exp/constraints"
)

// Make a chan for the packets and readfrom addr on a PacketConn
func Chan(pc net.PacketConn, bufSize int) (<-chan []byte, chan net.Addr) {
	msgs := make(chan []byte)
	addrs := make(chan net.Addr)
	go func() {
		for {
			buf := make([]byte, bufSize)
			n, addr, err := pc.ReadFrom(buf)
			if err != nil {
				log.Fatal(err)
			}
			msgs <- buf[:n]
			addrs <- addr
		}
		close(msgs)
		close(addrs)
	}()
	return msgs, addrs
}

type Key[T constraints.Ordered] struct {
	Type      string
	Timestamp T
}

func Latest[T constraints.Ordered](msgs <-chan []byte, get func(msg []byte) Key[T]) <-chan []byte {
	out := make(chan []byte)
	go func() {
		latest := make(map[string]T)
		var zeroT T
		for msg := range msgs {
			key := get(msg)
			if key.Timestamp == zeroT {
				log.Printf("message Key.Type %s with zero timestamp",
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
