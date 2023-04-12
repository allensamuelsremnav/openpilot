package net

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
