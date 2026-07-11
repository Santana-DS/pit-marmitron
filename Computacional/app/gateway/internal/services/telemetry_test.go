package services

import (
	"context"
	"io"
	"log/slog"
	"testing"
	"time"
)

type telemetryRepoStub struct {
	batches [][]TelemetryPoint
}

func (r *telemetryRepoStub) InsertBatch(_ context.Context, points []TelemetryPoint) error {
	copied := append([]TelemetryPoint(nil), points...)
	r.batches = append(r.batches, copied)
	return nil
}

func TestEnqueueTelemetryFiltersIdleTicks(t *testing.T) {
	repo := &telemetryRepoStub{}
	svc := NewTelemetryIngestService(repo, testTelemetryLogger())

	svc.EnqueueTelemetry([]byte(`{"timestamp":1714000000,"nav_state":"IDLE","active_order_id":""}`))

	if svc.skippedCounter.Load() != 1 {
		t.Fatalf("skippedCounter = %d, want 1", svc.skippedCounter.Load())
	}
	if len(svc.telemetryChan) != 0 {
		t.Fatalf("telemetryChan len = %d, want 0", len(svc.telemetryChan))
	}
}

func TestEnqueueTelemetryMapsDaemonPayload(t *testing.T) {
	repo := &telemetryRepoStub{}
	svc := NewTelemetryIngestService(repo, testTelemetryLogger())

	svc.EnqueueTelemetry([]byte(`{
		"source":"edge_daemon",
		"timestamp":1714000000,
		"pose":{"x":1.25,"y":-2.5,"theta":0.75,"frame":"map"},
		"velocity":{"linear_mps":0.42},
		"battery":{"percent":88.5,"voltage_v":24.2},
		"nav_state":"NAVIGATING",
		"active_order_id":"order_1714000000123",
		"remaining_m":12.3,
		"progress_pct":45.6,
		"avg_speed_mps":0.39,
		"eta_seconds":31.5,
		"cpu_pct":10.1,
		"mem_pct":20.2
	}`))

	select {
	case pt := <-svc.telemetryChan:
		if pt.OrderID != "order_1714000000123" {
			t.Fatalf("OrderID = %q", pt.OrderID)
		}
		if !pt.Ts.Equal(time.Unix(1714000000, 0).UTC()) {
			t.Fatalf("Ts = %s", pt.Ts)
		}
		if pt.NavState != "NAVIGATING" || pt.PoseX != 1.25 || pt.PoseY != -2.5 || pt.PoseTheta != 0.75 {
			t.Fatalf("unexpected mapped point: %#v", pt)
		}
		if pt.LinearSpeedMps != 0.42 || pt.BatteryPercent != 88.5 || pt.CpuPct != 10.1 {
			t.Fatalf("unexpected metrics: %#v", pt)
		}
	default:
		t.Fatal("expected telemetry point to be queued")
	}
}

func TestRunBatcherFlushesOnShutdown(t *testing.T) {
	repo := &telemetryRepoStub{}
	svc := NewTelemetryIngestService(repo, testTelemetryLogger())
	ctx, cancel := context.WithCancel(context.Background())
	done := make(chan struct{})

	go func() {
		defer close(done)
		svc.RunBatcher(ctx)
	}()

	svc.EnqueueTelemetry([]byte(`{"timestamp":1714000000,"nav_state":"NAVIGATING","active_order_id":"order_1"}`))
	cancel()

	select {
	case <-done:
	case <-time.After(time.Second):
		t.Fatal("batcher did not stop")
	}

	if len(repo.batches) != 1 {
		t.Fatalf("batches = %d, want 1", len(repo.batches))
	}
	if got := repo.batches[0][0].OrderID; got != "order_1" {
		t.Fatalf("flushed OrderID = %q", got)
	}
}

func testTelemetryLogger() *slog.Logger {
	return slog.New(slog.NewTextHandler(io.Discard, nil))
}
