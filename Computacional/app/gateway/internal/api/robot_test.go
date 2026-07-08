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
	t.Setenv("ROBOT_CAMERA_SIGNALING_URL", "")
	t.Setenv("ROBOT_CAMERA_ICE_SERVERS", "")
	t.Setenv("ROBOT_CAMERA_LABEL", "")
	t.Setenv("ROBOT_CAMERA_LATENCY_TARGET_MS", "")
	t.Setenv("ROBOT_CAMERA_ROS_IMAGE_TOPIC", "")
	t.Setenv("ROBOT_CAMERA_ROS_COMPRESSED_TOPIC", "")

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
	if got.Label != "C920 front camera" {
		t.Fatalf("label = %q, want C920 front camera", got.Label)
	}
	if got.LatencyTargetMS != 500 {
		t.Fatalf("latency_target_ms = %d, want 500", got.LatencyTargetMS)
	}
	if got.ROSImageTopic != "/camera/image_raw" {
		t.Fatalf("ros_image_topic = %q, want /camera/image_raw", got.ROSImageTopic)
	}
	if got.ROSCompressedTopic != "/camera/image_raw/compressed" {
		t.Fatalf("ros_compressed_topic = %q, want /camera/image_raw/compressed", got.ROSCompressedTopic)
	}
}

func TestCameraConfigHandlerConfigured(t *testing.T) {
	t.Setenv("ROBOT_CAMERA_STREAM_URL", "https://relay.example/unbot/camera/front")
	t.Setenv("ROBOT_CAMERA_STREAM_KIND", "webrtc")
	t.Setenv("ROBOT_CAMERA_SIGNALING_URL", "wss://relay.example/unbot/signaling")
	t.Setenv("ROBOT_CAMERA_ICE_SERVERS", "stun:stun.l.google.com:19302, turn:relay.example")
	t.Setenv("ROBOT_CAMERA_LABEL", "C920 front camera")
	t.Setenv("ROBOT_CAMERA_LATENCY_TARGET_MS", "350")
	t.Setenv("ROBOT_CAMERA_ROS_IMAGE_TOPIC", "/camera/front/image_raw")
	t.Setenv("ROBOT_CAMERA_ROS_COMPRESSED_TOPIC", "/camera/front/image_raw/compressed")

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
	if got.StreamURL != "https://relay.example/unbot/camera/front" {
		t.Fatalf("stream_url = %q", got.StreamURL)
	}
	if got.StreamKind != "webrtc" {
		t.Fatalf("stream_kind = %q, want webrtc", got.StreamKind)
	}
	if got.SignalingURL != "wss://relay.example/unbot/signaling" {
		t.Fatalf("signaling_url = %q", got.SignalingURL)
	}
	if len(got.ICEServers) != 2 {
		t.Fatalf("ice_servers len = %d, want 2", len(got.ICEServers))
	}
	if got.Label != "C920 front camera" {
		t.Fatalf("label = %q, want C920 front camera", got.Label)
	}
	if got.LatencyTargetMS != 350 {
		t.Fatalf("latency_target_ms = %d, want 350", got.LatencyTargetMS)
	}
	if got.ROSImageTopic != "/camera/front/image_raw" {
		t.Fatalf("ros_image_topic = %q", got.ROSImageTopic)
	}
	if got.ROSCompressedTopic != "/camera/front/image_raw/compressed" {
		t.Fatalf("ros_compressed_topic = %q", got.ROSCompressedTopic)
	}
}
