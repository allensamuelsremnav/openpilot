package gpsd

import (
	"bufio"
	"bytes"
	"testing"
)

func TestConcat(t *testing.T) {
	logs := []string{"gpsd.rn3", "gpsd.rn5.log"}
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
	logs := []string{"nonl.json", "gpsd.rn5.log"}
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
