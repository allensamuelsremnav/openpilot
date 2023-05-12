package net

import (
	"encoding/json"
	"log"

	"golang.org/x/exp/constraints"
)

type Key[T constraints.Ordered] struct {
	Type      string
	Timestamp T
}

// Keep only messages that are strictly after any timestamp that we've
// seen so far.  Within Key.Type.
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

// Forward messages that are newer than any previous message according the field key.
func LatestInt64(msgs <-chan []byte, key string) <-chan []byte {
	out := make(chan []byte)
	go func() {
		var latest int64
		for msg := range msgs {
			var f interface{}
			err := json.Unmarshal(msg, &f)
			if err != nil {
				log.Fatal(err)
			}
			m := f.(map[string]interface{})
			v, ok := m[key]
			if ok {
				ts := int64(v.(float64))
				if ts > latest {
					latest = ts
					out <- msg
				}
			}
		}
		close(out)
	}()
	return out
}
