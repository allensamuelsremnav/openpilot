package net

import (
	"strings"
	"testing"
)

// Extract Key from comma-separated string: "type,timestamp,...."
func getter(s string) Key {
	tokens := strings.Split(s, ",")
	return Key{Type: tokens[0], Timestamp: tokens[1]}
}

// Pass raw strings through Latest filter.
func pump(raw []string) []string {
	rawch := make(chan string)
	go func() {
		for _, r := range raw {
			rawch <- r
		}
		close(rawch)
	}()
	var cooked []string
	for f := range Latest(rawch, getter) {
		cooked = append(cooked, f)
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
