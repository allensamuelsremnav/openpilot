package gpsd

import (
	"fmt"
	"testing"
	"time"
)

func inlineTime(t *testing.T, s string) time.Time {
	// Check and discard error return.
	tm, err := time.Parse(time.RFC3339, s)
	if err != nil {
		t.Fatal(err)
	}
	return tm
}

// got-want checkers
func checkWantNil(t *testing.T, tag string, got *Position, got_index int) {
	var want *Position
	if want != got {
		t.Fatalf("%s, got %v, want %v", tag, got, want)
	}
	if got_index > 0 {
		t.Fatalf("%s, got %d, want negative value", tag, got_index)
	}
	fmt.Println(tag, got_index)
}

func checkWant(t *testing.T, tag string, want Position, want_index int, got *Position, got_index int) {
	if want != *got {
		t.Fatalf("%s, got %v, want %v", tag, *got, want)
	}
	if want_index != got_index {
		t.Fatalf("%s, got %d, want %d", tag, got_index, want_index)
	}
	fmt.Println(tag, got_index)
}

func TestInterval(t *testing.T) {
	// Test TPV sequences and probe points.
	inline := func(s string) time.Time {
		return inlineTime(t, s)
	}
	tpvs := []TPV{
		TPV{Time: inline("2022-10-01T12:00:10Z"),
			Lat:   10,
			Lon:   -100,
			Speed: 1},
		TPV{Time: inline("2022-10-01T12:00:20Z"),
			Lat:   20,
			Lon:   -200,
			Speed: 2},
		TPV{Time: inline("2022-10-01T12:00:30Z"),
			Lat:   30,
			Lon:   -300,
			Speed: 3}}
	// One TPV, no intervals
	pos, index := Interpolate(tpvs[:1], inline("2022-10-01T12:00:00Z"), 0)
	checkWantNil(t, "one TPV", pos, index)

	// Two TPV, one interval
	pos, index = Interpolate(tpvs[:2], inline("2022-10-01T12:00:09Z"), 0)
	checkWantNil(t, "probe early, 2 TPV", pos, index)
	pos, index = Interpolate(tpvs[:2], inline("2022-10-01T12:00:21Z"), 0)
	checkWantNil(t, "probe late, 2 TPV", pos, index)

	pos, index = Interpolate(tpvs[:2], inline("2022-10-01T12:00:10Z"), 0)
	checkWant(t, "interpolate, 2 TPV, start", Position{Time: inline("2022-10-01T12:00:10Z"),
		Lat: 10, Lon: -100, Speed: 1}, 0, pos, index)

	pos, index = Interpolate(tpvs[:2], inline("2022-10-01T12:00:15Z"), 0)
	checkWant(t, "interpolate, 2 TPV, mid", Position{Time: inline("2022-10-01T12:00:15Z"),
		Lat: 15, Lon: -150, Speed: 1.5}, 0, pos, index)

	pos, index = Interpolate(tpvs[:2], inline("2022-10-01T12:00:20Z"), 0)
	checkWant(t, "interpolate, 2 TPV, end", Position{Time: inline("2022-10-01T12:00:20Z"),
		Lat: 20, Lon: -200, Speed: 2}, 0, pos, index)

	// Three TPV, two intervals
	pos, index = Interpolate(tpvs[:3], inline("2022-10-01T12:00:09Z"), 0)
	checkWantNil(t, "probe early, 3 TPV", pos, index)
	pos, index = Interpolate(tpvs[:3], inline("2022-10-01T12:00:31Z"), 0)
	checkWantNil(t, "probe late, 3 TPV", pos, index)

	pos, index = Interpolate(tpvs[:3], inline("2022-10-01T12:00:10Z"), 0)
	checkWant(t, "interpolate, 0/2 intervals, begin, 3 TPV", Position{Time: inline("2022-10-01T12:00:10Z"),
		Lat: 10, Lon: -100, Speed: 1}, 0, pos, index)
	pos, index = Interpolate(tpvs[:3], inline("2022-10-01T12:00:16Z"), 0)
	checkWant(t, "interpolate, 0/2 interval, mid, 3 TPV", Position{Time: inline("2022-10-01T12:00:16Z"),
		Lat: 16, Lon: -160, Speed: 1.6}, 0, pos, index)
	pos, index = Interpolate(tpvs[:3], inline("2022-10-01T12:00:20Z"), 0)
	checkWant(t, "interpolate, 0/2 interval, end, 3 TPV", Position{Time: inline("2022-10-01T12:00:20Z"),
		Lat: 20, Lon: -200, Speed: 2}, 0, pos, index)

	pos, index = Interpolate(tpvs[:3], inline("2022-10-01T12:00:24Z"), 1)
	checkWant(t, "interpolate, 1/2 interval, mid, 3 TPV", Position{Time: inline("2022-10-01T12:00:24Z"),
		Lat: 24, Lon: -240, Speed: 2.4}, 1, pos, index)
	pos, index = Interpolate(tpvs[:3], inline("2022-10-01T12:00:30Z"), 1)
	checkWant(t, "interpolate, 1/2 interval, end, 3 TPV", Position{Time: inline("2022-10-01T12:00:30Z"),
		Lat: 30, Lon: -300, Speed: 3}, 1, pos, index)

}
