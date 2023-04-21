package net

import (
	"log"
	"strconv"
	"strings"
	"testing"
)

// Extract Key from comma-separated string: "type,timestamp,...."
func getter(b []byte) Key[int64] {
	tokens := strings.Split(string(b), ",")
	ts, ok := strconv.ParseInt(tokens[1], 10, 64)
	if ok != nil {
		log.Fatal(ok)
	}
	return Key[int64]{Type: tokens[0], Timestamp: ts}
}

// Pass raw strings through Latest filter.
func pump(raw []string) []string {
	rawch := make(chan []byte)
	go func() {
		for _, r := range raw {
			rawch <- []byte(r)
		}
		close(rawch)
	}()
	var cooked []string
	for f := range Latest(rawch, getter) {
		cooked = append(cooked, string(f))
	}
	return cooked
}

func check(t *testing.T, want, got []string) {
	if len(want) != len(got) {
		t.Fatalf("got len(got) = %d, want len(want) = %d",
			len(got), len(want))
	}
	for i, w := range want {
		if w != got[i] {
			t.Fatalf("got %s, want %s", got[i], w)
		}
	}
}

// Types are handled separately.
func TestTypes(t *testing.T) {
	raw := []string{
		"a,1,x", "b,1,x", "a,1,z",
		"a,3,y", "b,2,z", "a,2,z", "a,4,z"}
	want := []string{
		"a,1,x", "b,1,x",
		"a,3,y", "b,2,z",
		"a,4,z"}
	got := pump(raw)
	check(t, want, got)
}

func TestOutOfOrder(t *testing.T) {
	raw := []string{
		"a,2,x", "a,1,x"}
	want := []string{
		"a,2,x",
	}
	got := pump(raw)
	check(t, want, got)
}

func TestDups(t *testing.T) {
	raw := []string{
		"a,1,z", "a,1,y",
		"a,2,x", "a,1,x",
		"a,2,y"}
	want := []string{
		"a,1,z",
		"a,2,x",
	}
	got := pump(raw)
	check(t, want, got)
}

func TestZero(t *testing.T) {
	raw := []string{
		"a,1,z", "a,0,y",
		"a,2,x"}
	want := []string{
		"a,1,z",
		"a,2,x",
	}
	got := pump(raw)
	check(t, want, got)
}
