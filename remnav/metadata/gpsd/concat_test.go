package gpsd

import (
	"bufio"
	"bytes"
	"strings"
	"testing"
	"time"
)

func TestConcat(t *testing.T) {
	logs := []string{"gpsd.rn3_g000.json", "gpsd.rn5_g000.json"}
	var b bytes.Buffer
	gotWritten := Concat(logs, &b)
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
	//logs := []string{"gpsd.rn6.nonl_g000.json", "gpsd.rn5_g000.json"}
	for _, logs := range [][]string{
		{"gpsd.nonl_g000.json", "gpsd.rn5_g000.json"},
		{"gpsd.rn6.nonl_g000.json", "gpsd.rn5_g000.json"}} {
		var b bytes.Buffer
		gotWritten := Concat(logs, &b)
		wantLen := 9808842
		gotLen := b.Len()
		if gotWritten != wantLen {
			t.Fatalf("gotWritten %d != %d", gotWritten, wantLen)
		}
		if gotLen != wantLen {
			t.Fatalf("gotLen %d != %d", gotLen, wantLen)
		}
	}
}

func TestParseCheck(t *testing.T) {
	{
		b := []byte(`{"class":"TPV","device":"/dev/ttyACM0","status":2,"mode":3,"time":"2022-10-20T20:32:53.000Z","leapseconds":18,"ept":0.005,"lat":37.270564937,"lon":-121.771497228,"altHAE":177.4042,"altMSL":209.3529,"alt":209.3529,"epx":2.678,"epy":2.761,"epv":10.079,"track":306.2339,"magtrack":319.2934,"magvar":13.1,"speed":0.170,"climb":-0.083,"eps":0.50,"epc":20.16,"ecefx":-2675849.16,"ecefy":-4320502.65,"ecefz":3841445.28,"ecefvx":0.00,"ecefvy":-0.06,"ecefvz":-0.01,"ecefpAcc":7.83,"ecefvAcc":0.50,"velN":0.101,"velE":-0.137,"velD":0.083,"geoidSep":-31.949,"eph":4.818,"sep":9.620}
{"class":"TPV","device":"/dev/ttyACM0","status":2,"mode":3,"time":"2022-10-20T20:32:52.000Z","leapseconds":18,"ept":0.005,"lat":37.270564937,"lon":-121.771497228,"altHAE":177.4042,"altMSL":209.3529,"alt":209.3529,"epx":2.678,"epy":2.761,"epv":10.079,"track":306.2339,"magtrack":319.2934,"magvar":13.1,"speed":0.170,"climb":-0.083,"eps":0.50,"epc":20.16,"ecefx":-2675849.16,"ecefy":-4320502.65,"ecefz":3841445.28,"ecefvx":0.00,"ecefvy":-0.06,"ecefvz":-0.01,"ecefpAcc":7.83,"ecefvAcc":0.50,"velN":0.101,"velE":-0.137,"velD":0.083,"geoidSep":-31.949,"eph":4.818,"sep":9.620}
`)
		rdr := bufio.NewReader(bytes.NewBuffer(b))
		err := ParseCheck(rdr)
		if !strings.Contains(err.Error(), "out-of-order timestamp 2022-10-20 20:32:52") {
			t.Fatal(err)
		}
	}
	{
		b := []byte(`{"class":"TPV","device":"/dev/ttyACM0","status":2,"mode":3,"time":"2022-10-20T20:32:53.000Z","leapseconds":18,"ept":0.005,"lat":37.270564937,"lon":-121.771497228,"altHAE":177.4042,"altMSL":209.3529,"alt":209.3529,"epx":2.678,"epy":2.761,"epv":10.079,"track":306.2339,"magtrack":319.2934,"magvar":13.1,"speed":0.170,"climb":-0.083,"eps":0.50,"epc":20.16,"ecefx":-2675849.16,"ecefy":-4320502.65,"ecefz":3841445.28,"ecefvx":0.00,"ecefvy":-0.06,"ecefvz":-0.01,"ecefpAcc":7.83,"ecefvAcc":0.50,"velN":0.101,"velE":-0.137,"velD":0.083,"geoidSep":-31.949,"eph":4.818,"sep":9.620}{"class":"TPV","device":"/dev/ttyACM0","status":2,"mode":3,"time":"2022-10-20T20:32:52.000Z","leapseconds":18,"ept":0.005,"lat":37.270564937,"lon":-121.771497228,"altHAE":177.4042,"altMSL":209.3529,"alt":209.3529,"epx":2.678,"epy":2.761,"epv":10.079,"track":306.2339,"magtrack":319.2934,"magvar":13.1,"speed":0.170,"climb":-0.083,"eps":0.50,"epc":20.16,"ecefx":-2675849.16,"ecefy":-4320502.65,"ecefz":3841445.28,"ecefvx":0.00,"ecefvy":-0.06,"ecefvz":-0.01,"ecefpAcc":7.83,"ecefvAcc":0.50,"velN":0.101,"velE":-0.137,"velD":0.083,"geoidSep":-31.949,"eph":4.818,"sep":9.620}
`)
		rdr := bufio.NewReader(bytes.NewBuffer(b))
		err := ParseCheck(rdr)
		if !strings.Contains(err.Error(), "invalid character '{' after top-level value") {
			t.Fatal(err)
		}

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
