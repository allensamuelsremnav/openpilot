package net

import (
	"log"

	"golang.org/x/exp/constraints"
)

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
