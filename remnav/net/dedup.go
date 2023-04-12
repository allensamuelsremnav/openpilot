package net

type Key struct {
	Type string
	// Must be lexicographically ordered.
	Timestamp string
}

// The returned channel will contain any msg of a given Type whose
// Timestamp is later than the Timestamp of any msg with the same Type
// that was already written to the channel. Equivalently: for each
// Type, out-of-order or redundant messages are dropped.
func Latest(msgs <-chan string, get func(msg string) Key) <-chan string {
	out := make(chan string)
	go func() {
		latest := make(map[string]string)
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
