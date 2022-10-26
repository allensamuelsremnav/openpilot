package gpsd

import (
	"bufio"
	"encoding/json"
	"io"
	"math"
	"os"
	"testing"
	"time"
)

var js = []string{
	`{"class":"DEVICES","devices":[{"class":"DEVICE","path":"/dev/ttyACM0","driver":"u-blox","subtype":"SW 1.00 (59842),HW 00070000","subtype1":"PROTVER 14.00,GPS;SBAS;GLO;QZSS","activated":"2022-10-20T20:32:52.071Z","flags":1,"native":1,"bps":9600,"parity":"N","stopbits":1,"cycle":1.00,"mincycle":0.02},{"class":"DEVICE","path":"/dev/pps0","driver":"PPS","activated":"2022-10-20T20:32:52.000Z"}]}`,
	`{"class":"WATCH","enable":true,"json":true,"nmea":false,"raw":0,"scaled":false,"timing":false,"split24":false,"pps":false}`,
	`{"class":"TPV","device":"/dev/ttyACM0","status":2,"mode":3,"time":"2022-10-20T20:32:53.000Z","leapseconds":18,"ept":0.005,"lat":37.270564937,"lon":-121.771497228,"altHAE":177.4042,"altMSL":209.3529,"alt":209.3529,"epx":2.678,"epy":2.761,"epv":10.079,"track":306.2339,"magtrack":319.2934,"magvar":13.1,"speed":0.170,"climb":-0.083,"eps":0.50,"epc":20.16,"ecefx":-2675849.16,"ecefy":-4320502.65,"ecefz":3841445.28,"ecefvx":0.00,"ecefvy":-0.06,"ecefvz":-0.01,"ecefpAcc":7.83,"ecefvAcc":0.50,"velN":0.101,"velE":-0.137,"velD":0.083,"geoidSep":-31.949,"eph":4.818,"sep":9.620}`,
	`{"class":"SKY","device":"/dev/ttyACM0","time":"2022-10-20T20:42:30.000Z","xdop":0.71,"ydop":0.71,"vdop":2.00,"tdop":1.32,"hdop":1.01,"gdop":2.60,"pdop":2.24,"nSat":18,"uSat":9,"satellites":[{"PRN":1,"el":51.0,"az":238.0,"ss":0.0,"used":false,"gnssid":0,"svid":1,"health":1},{"PRN":3,"el":58.0,"az":322.0,"ss":31.0,"used":true,"gnssid":0,"svid":3,"health":1},{"PRN":4,"el":34.0,"az":273.0,"ss":22.0,"used":true,"gnssid":0,"svid":4,"health":1},{"PRN":9,"el":1.0,"az":263.0,"ss":0.0,"used":false,"gnssid":0,"svid":9,"health":1},{"PRN":16,"el":5.0,"az":154.0,"ss":0.0,"used":false,"gnssid":0,"svid":16,"health":1},{"PRN":17,"el":1.0,"az":293.0,"ss":0.0,"used":false,"gnssid":0,"svid":17,"health":1},{"PRN":21,"el":38.0,"az":212.0,"ss":23.0,"used":true,"gnssid":0,"svid":21,"health":1},{"PRN":22,"el":38.0,"az":55.0,"ss":39.0,"used":true,"gnssid":0,"svid":22,"health":1},{"PRN":25,"el":5.0,"az":38.0,"ss":14.0,"used":false,"gnssid":0,"svid":25,"health":1},{"PRN":26,"el":22.0,"az":130.0,"ss":28.0,"used":true,"gnssid":0,"svid":26,"health":1},{"PRN":31,"el":55.0,"az":59.0,"ss":42.0,"used":true,"gnssid":0,"svid":31,"health":1},{"PRN":32,"el":13.0,"az":69.0,"ss":29.0,"used":true,"gnssid":0,"svid":32,"health":1},{"PRN":46,"el":46.0,"az":192.0,"ss":34.0,"used":true,"gnssid":1,"svid":133,"health":1},{"PRN":48,"el":47.0,"az":185.0,"ss":30.0,"used":true,"gnssid":1,"svid":135,"health":1},{"PRN":51,"el":44.0,"az":157.0,"ss":0.0,"used":false,"gnssid":1,"svid":138,"health":1},{"PRN":193,"az":0.0,"ss":0.0,"used":false,"gnssid":5,"svid":1,"health":1},{"PRN":195,"az":0.0,"ss":0.0,"used":false,"gnssid":5,"svid":3,"health":1},{"PRN":196,"az":0.0,"ss":0.0,"used":false,"gnssid":5,"svid":4,"health":1}]}`,
	`{"class":"PPS","device":"/dev/pps0","real_sec":1666298551,"real_nsec":0,"clock_sec":1666298551,"clock_nsec":1881,"precision":-20}
`}

func checkFloat64(t *testing.T, field string, want, got float64) {
	EPS := 0.000000001
	if math.Abs(want-got) > EPS {
		t.Fatalf("got %s = %.10f, want %0.10f", field, got, want)
	}
}
func checkTime(t *testing.T, field string, want string, got time.Time) {
	w, err := time.Parse(time.RFC3339, want)
	if err != nil {
		t.Fatal(err)
	}
	if !w.Equal(got) {
		t.Fatalf("got %s = %s, want %s", field, got, w)
	}
}
func checkString(t *testing.T, field string, want, got string) {
	if want != got {
		t.Fatalf("got %s = %s, want %s", field, got, want)
	}
}
func checkInt(t *testing.T, field string, want int, got int) {
	if want != got {
		t.Fatalf("got %s = %d, want %d", field, got, want)
	}
}
func TestUnmarshal(t *testing.T) {
	// Test unmarshalling of the expected messages. Unmarshalling
	// should be perfect except for human mistakes in
	// specification.
	for _, j := range js {
		// Use TPV for probe since it has a class field
		var probe class
		err := json.Unmarshal([]byte(j), &probe)
		if err != nil {
			t.Fatal(err)
		}
		if probe.Class == "TPV" {
			var tpv TPV
			err := json.Unmarshal([]byte(j), &tpv)
			if err != nil {
				t.Fatal(err)
			}
			checkString(t, "Device", "/dev/ttyACM0", tpv.Device)
			if want := Mode3D; want != tpv.Mode {
				t.Fatalf("got %s = %d, want %d", "Mode", tpv.Mode, want)
			}
			checkTime(t, "Time", "2022-10-20T20:32:53.000Z", tpv.Time)
			checkFloat64(t, "Lat", 37.270564937, tpv.Lat)
			checkFloat64(t, "Lon", -121.771497228, tpv.Lon)
			checkFloat64(t, "Alt", 209.3529, tpv.Alt)
			checkFloat64(t, "EPX", 2.678, tpv.EPX)
			checkFloat64(t, "EPY", 2.761, tpv.EPY)
			checkFloat64(t, "EPV", 10.079, tpv.EPV)
			checkFloat64(t, "SEP", 9.620, tpv.SEP)
			checkFloat64(t, "Track", 306.2339, tpv.Track)
			checkFloat64(t, "Speed", 0.170, tpv.Speed)
			checkFloat64(t, "EPS", 0.50, tpv.EPS)
		} else if probe.Class == "SKY" {
			var sky SKY
			err := json.Unmarshal([]byte(j), &sky)
			if err != nil {
				t.Fatal(err)
			}

			checkString(t, "Device", "/dev/ttyACM0", sky.Device)
			checkTime(t, "Time", "2022-10-20T20:42:30.000Z", sky.Time)
			checkFloat64(t, "XDOP", 0.71, sky.XDOP)
			checkFloat64(t, "YDOP", 0.71, sky.YDOP)
			checkFloat64(t, "VDOP", 2.00, sky.VDOP)
			checkFloat64(t, "HDOP", 1.01, sky.HDOP)
			checkFloat64(t, "PDOP", 2.24, sky.PDOP)
			used := 0
			for _, prn := range sky.Satellites {
				if prn.Used {
					used += 1
				}
			}
			// fmt.Printf("%s %v, %d / %d PRN used\n", probe.Class, sky.Time, used, len(sky.Satellites))
			checkInt(t, "used", 9, used)
			checkInt(t, "len(Satellites)", 18, len(sky.Satellites))
		} else if probe.Class == "PPS" {
			var pps PPS
			err := json.Unmarshal([]byte(j), &pps)
			if err != nil {
				t.Fatal(err)
			}
			checkString(t, "Device", "/dev/pps0", pps.Device)
			if want := "/dev/pps0"; pps.Device != want {
				t.Fatalf("pps device = %s, want %s", pps.Device, want)
			}
			checkInt(t, "RealSec", 1666298551, pps.RealSec)
			checkInt(t, "RealNsec", 0, pps.RealNsec)
			checkInt(t, "ClockSec", 1666298551, pps.ClockSec)
			checkInt(t, "ClockNsec", 1881, pps.ClockNsec)
		} else if probe.Class == "DEVICES" {
			var devices Devices
			err := json.Unmarshal([]byte(j), &devices)
			if err != nil {
				t.Fatal(err)
			}
			want := []string{"/dev/ttyACM0", "/dev/pps0"}
			for i, d := range devices.Devices {
				checkString(t, "path", want[i], d.Path)
			}
		} else if probe.Class == "WATCH" {
			continue
		} else {
			t.Fatalf("%s unexpected", probe.Class)
		}
	}
}

func TestLog(t *testing.T) {
	// Test reading a sample log file.
	f, err := os.Open("gpsd.rn3")
	if err != nil {
		t.Fatal(err)
	}
	rdr := bufio.NewReader(f)
	counts := make(map[string]int)
	for {
		line, err := rdr.ReadString('\n')
		if err == io.EOF {
			break
		}
		if err != nil {
			t.Fatal(err)
		}
		var tpv TPV
		err = json.Unmarshal([]byte(line), &tpv)
		if err != nil {
			t.Fatal(err)
		}
		i := counts[tpv.Class]
		counts[tpv.Class] = i + 1
	}
	want := map[string]int{"DEVICES": 1, "PPS": 579, "SKY": 577, "TPV": 868, "WATCH": 1}
	if len(want) != len(counts) {
		t.Fatalf("len(counts) = %d, want %d", len(counts), len(want))
	}
	for k, w := range want {
		got, ok := counts[k]
		if !ok {
			t.Fatalf("key %s not found", k)
		}
		if got != w {
			t.Fatalf("got counts[%s] == %d, want %d", k, got, w)
		}
	}
}
