package trajectory

import (
	"math"
	"testing"
)

var params Parameters

func setup() {
	params = Parameters{Wheelbase: 4,
		GameWheel2Tire: math.Pi, // One full turn lock-to-lock
		TireMax:        math.Pi / 2,
		DtireDtMax:     0.25,
		Interval:       50,
	}
}

func TestCurvature(t *testing.T) {
	setup()
	for _, aw := range []struct {
		arg  float64
		want float64
	}{
		{math.Pi / 2, 4},
		{-math.Pi / 2, -4},
		{math.Pi / 4, 5.65685424949238019521},
		{-math.Pi / 4, -5.65685424949238019521},
		{math.Pi / 100, 127.34490083639049182265},
		{-math.Pi / 100, -127.34490083639049182265},
	} {
		got := params.curvature(aw.arg)
		if math.Abs(got-aw.want) > 1e-17 {
			t.Fatalf("got %v, want %v for case %v", got, aw.want, aw)
		}
	}
}

func TestTire(t *testing.T) {
	setup()
	for _, aw := range []struct {
		arg  float64
		want float64
	}{
		{0.0, 0.0},
		{0.01, math.Pi / 100},
		{-0.01, -math.Pi / 100},
		{1.0, math.Pi / 2},
		{-1.0, -math.Pi / 2},
		{0.5, math.Pi / 2},
		{-0.5, -math.Pi / 2},
	} {
		got := params.tire(aw.arg)
		if got != aw.want {
			t.Fatalf("got %v, want %v for arg %v", got, aw.want, aw.arg)
		}
	}
}

func TestLimitDtireDt(t *testing.T) {
	setup()
	for _, aw := range []struct {
		prev      float64
		requested float64
		dt        float64
		want      float64
	}{
		{0.0, 0.0, 0.0, 0.0},
		{0.0, 0.01, 2.0, 0.01},
		{0.0, 0.49, 2.0, 0.49},
		{0.0, 0.5, 2.0, 0.5},
		{0.0, 0.51, 2.0, 0.5},
		{0.0, 1.0, 2.0, 0.5},
		{0.0, -0.01, 2.0, -0.01},
		{0.0, -0.49, 2.0, -0.49},
		{0.0, -0.5, 2.0, -0.5},
		{0.0, -0.51, 2.0, -0.5},
		{0.0, -1.0, 2.0, -0.5},
		{0.1, 0.2, 2.0, 0.2},
		{0.1, 0.2, 0.25, 0.1625}, // 0.1 + 0.25*0.25
		{-0.1, -0.2, 2.0, -0.2},
		{-0.1, -0.2, 0.25, -0.1625},
		{0.1, -0.1, 0.25, 0.1 - 0.25*0.25},
		{-0.1, 0.1, 3.0, 0.1},
		{-0.1, 0.1, 0.5, -0.1 + 0.25*0.5},
	} {
		got := params.limitDtireDt(aw.prev, aw.requested, aw.dt)
		if math.Abs(got-aw.want) > 1e-17 {
			t.Fatalf("got %v, want %v for case %v", got, aw.want, aw)
		}
	}

}
