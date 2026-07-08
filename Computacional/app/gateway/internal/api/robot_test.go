package api

import (
	"encoding/json"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"
)

func TestCameraConfigHandlerUnset(t *testing.T) {
	t.Setenv("ROBOT_CAMERA_STREAM_URL", "")
	t.Setenv("ROBOT_CAMERA_STREAM_KIND", "")
	t.Setenv("ROBOT_CAMERA_LABEL", "")
	t.Setenv("ROBOT_CAMERA_LATENCY_TARGET_MS", "")

	s := &Server{log: slog.Default()}
	req := httptest.NewRequest(http.MethodGet, "/api/robot/camera", nil)
	rr := httptest.NewRecorder()

	s.cameraConfigHandler().ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rr.Code, http.StatusOK)
	}

	var got cameraConfigResponse
	if err := json.NewDecoder(rr.Body).Decode(&got); err != nil {
		t.Fatalf("decode response: %v", err)
	}
	if got.Available {
		t.Fatal("available = true, want false")
	}
	if got.StreamKind != "unset" {
		t.Fatalf("stream_kind = %q, want unset", got.StreamKind)
	}
	if got.Label != "Robot camera" {
		t.Fatalf("label = %q, want Robot camera", got.Label)
	}
	if got.LatencyTargetMS != 500 {
		t.Fatalf("latency_target_ms = %d, want 500", got.LatencyTargetMS)
	}
}

func TestCameraConfigHandlerConfigured(t *testing.T) {
	t.Setenv("ROBOT_CAMERA_STREAM_URL", "http://example.local/stream.mjpg")
	t.Setenv("ROBOT_CAMERA_STREAM_KIND", "mjpeg")
	t.Setenv("ROBOT_CAMERA_LABEL", "C920 front camera")
	t.Setenv("ROBOT_CAMERA_LATENCY_TARGET_MS", "350")

	s := &Server{log: slog.Default()}
	req := httptest.NewRequest(http.MethodGet, "/api/robot/camera", nil)
	rr := httptest.NewRecorder()

	s.cameraConfigHandler().ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rr.Code, http.StatusOK)
	}

	var got cameraConfigResponse
	if err := json.NewDecoder(rr.Body).Decode(&got); err != nil {
		t.Fatalf("decode response: %v", err)
	}
	if !got.Available {
		t.Fatal("available = false, want true")
	}
	if got.StreamURL != "http://example.local/stream.mjpg" {
		t.Fatalf("stream_url = %q", got.StreamURL)
	}
	if got.StreamKind != "mjpeg" {
		t.Fatalf("stream_kind = %q, want mjpeg", got.StreamKind)
	}
	if got.Label != "C920 front camera" {
		t.Fatalf("label = %q, want C920 front camera", got.Label)
	}
	if got.LatencyTargetMS != 350 {
		t.Fatalf("latency_target_ms = %d, want 350", got.LatencyTargetMS)
	}
}
