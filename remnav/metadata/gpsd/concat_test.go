package gpsd

import (
	"bufio"
	"bytes"
	"testing"
	"time"
)

func TestConcat(t *testing.T) {
	logs := []string{"gpsd.rn3_g000.json", "gpsd.rn5_g000.json"}
	var b bytes.Buffer
	bw := bufio.NewWriter(&b)
	gotWritten := Concat(logs, bw)
	wantLen := 1522363 + 9808842
	gotLen := b.Len()
	if gotWritten != wantLen {
		t.Fatalf("gotWritten %d != %d", gotWritten, wantLen)
	}
	if gotLen != wantLen {
		t.Fatalf("gotLen %d != %d", gotLen, wantLen)
	}
}

func TestNewline(t *testing.T) {
	// Missing newline
	logs := []string{"nonl_g000.json", "gpsd.rn5_g000.json"}
	var b bytes.Buffer
	bw := bufio.NewWriter(&b)
	gotWritten := Concat(logs, bw)
	wantLen := 1692 + 9808842 + 1
	gotLen := b.Len()
	if gotWritten != wantLen {
		t.Fatalf("gotWritten %d != %d", gotWritten, wantLen)
	}
	if gotLen != wantLen {
		t.Fatalf("gotLen %d != %d", gotLen, wantLen)
	}
}

func TestIntersectionSingleton(t *testing.T) {
	logs := []string{"20221202T1134Z_g000.json"}
	{
		// metadata after logs
		first := time.Date(2022, 12, 2, 11, 35, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 36, 0, 0, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 0 {
			t.Fatalf("len(got) == %d, want %d", len(got), 0)
		}
	}
	{
		// metadata before logs
		first := time.Date(2022, 12, 2, 11, 33, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 33, 59, 999999999, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 0 {
			t.Fatalf("len(got) == %d, want %d", len(got), 0)
		}
	}
	{
		// first during logs
		first := time.Date(2022, 12, 2, 11, 34, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 35, 0, 0, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 1 {
			t.Fatalf("len(got) == %d, want %d", len(got), 1)
		}
		if got[0] != logs[0] {
			t.Fatalf("%v != %v", got[0], logs[0])
		}
	}
	{
		// last during logs
		first := time.Date(2022, 12, 2, 11, 33, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 34, 0, 0, time.UTC)

		got := Intersection(logs, first, last, false)
		if len(got) != 1 {
			t.Fatalf("#1 len(got) == %d, want %d", len(got), 1)
		}
		if got[0] != logs[0] {
			t.Fatalf("%v != %v", got[0], logs[0])
		}
	}
}

func TestIntersection(t *testing.T) {
	logs := []string{"20221202T1134Z_g000.json", "20221202T1136Z_g000.json"}
	{
		// metadata after logs
		first := time.Date(2022, 12, 2, 11, 37, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 38, 0, 0, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 0 {
			t.Fatalf("len(got) == %d, want %d", len(got), 0)
		}
	}
	{
		// metadata before logs
		first := time.Date(2022, 12, 2, 11, 33, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 33, 59, 999999999, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 0 {
			t.Fatalf("len(got) == %d, want %d", len(got), 0)
		}
	}
	{
		// first during logs
		first := time.Date(2022, 12, 2, 11, 34, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 35, 0, 0, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 1 {
			t.Fatalf("len(got) == %d, want %d", len(got), 1)
		}
		if got[0] != logs[0] {
			t.Fatalf("%v != %v", got[0], logs[0])
		}
	}
	{
		// last during logs
		first := time.Date(2022, 12, 2, 11, 36, 59, 999999999, time.UTC)
		last := time.Date(2022, 12, 2, 11, 37, 0, 0, time.UTC)

		got := Intersection(logs, first, last, false)
		if len(got) != 1 {
			t.Fatalf("#1 len(got) == %d, want %d", len(got), 1)
		}
		if got[0] != logs[len(logs)-1] {
			t.Fatalf("%v != %v", got[0], logs[0])
		}
	}
	{
		// intersect missing log
		first := time.Date(2022, 12, 2, 11, 35, 0, 0, time.UTC)
		last := time.Date(2022, 12, 2, 11, 35, 59, 999999999, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 0 {
			t.Fatalf("len(got) == %d, want %d", len(got), 1)
		}
	}
	{
		// intersect log. touch first log.
		first := time.Date(2022, 12, 2, 11, 34, 59, 999999999, time.UTC)
		last := time.Date(2022, 12, 2, 11, 35, 0, 1, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 1 {
			t.Fatalf("len(got) == %d, want %d", len(got), 1)
		}
		if got[0] != logs[0] {
			t.Fatalf("%v != %v", got[0], logs[0])
		}
	}
	{
		// intersect log. touch second log
		first := time.Date(2022, 12, 2, 11, 35, 59, 999999999, time.UTC)
		last := time.Date(2022, 12, 2, 11, 36, 0, 1, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 1 {
			t.Fatalf("len(got) == %d, want %d", len(got), 1)
		}
		if got[0] != logs[1] {
			t.Fatalf("%v != %v", got[0], logs[0])
		}
	}
	{
		// intersect log. touch both logs.
		first := time.Date(2022, 12, 2, 11, 34, 59, 999999999, time.UTC)
		last := time.Date(2022, 12, 2, 11, 36, 0, 1, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 2 {
			t.Fatalf("len(got) == %d, want %d", len(got), 1)
		}
	}
	{
		// intersect log. overlap logs.
		first := time.Date(2022, 12, 2, 11, 33, 59, 999999999, time.UTC)
		last := time.Date(2022, 12, 2, 11, 37, 0, 1, time.UTC)
		got := Intersection(logs, first, last, false)
		if len(got) != 2 {
			t.Fatalf("len(got) == %d, want %d", len(got), 1)
		}
	}
}
