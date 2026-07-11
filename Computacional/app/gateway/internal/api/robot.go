// internal/api/robot.go
//
// Operator-facing robot control endpoints:
//
//	POST /api/robot/estop     — publishes robot/commands/estop (QoS 2)
//	GET  /api/robot/telemetry — returns last telemetry snapshot received
//
// ESTOP SAFETY CONTRACT
// ──────────────────────
// The estop command is published at MQTT QoS 2 (exactly-once) so the
// broker guarantees delivery if the robot is connected. If MQTT is down,
// the handler returns HTTP 502 and the Flutter client warns the operator
// that the robot may NOT have received the stop command.
//
// TELEMETRY STORAGE
// ─────────────────
// The MQTT client in mqtt/client.go writes the latest robot/telemetry
// payload into a shared *RobotState (pointer passed to both handlers).
// GET /api/robot/telemetry reads from it under a read-lock.
// If no snapshot has arrived since startup, the handler returns 204.
package api

import (
	"encoding/json"
	"fmt"
	"net/http"
	"os"
	"strconv"
	"strings"
	"sync"
	"time"

	"unbot-gateway/internal/mqtt"
)

// ─── RobotState ──────────────────────────────────────────────────────────────
//
// Shared mutable state updated by the MQTT telemetry handler and read by
// GET /api/robot/telemetry. Protected by a read/write mutex.
type RobotState struct {
	mu          sync.RWMutex
	lastPayload []byte    // raw JSON from robot/telemetry
	receivedAt  time.Time // zero value = no snapshot yet
}

// Store saves a new raw telemetry payload (called from MQTT handler).
func (rs *RobotState) Store(payload []byte) {
	rs.mu.Lock()
	defer rs.mu.Unlock()
	rs.lastPayload = payload
	rs.receivedAt = time.Now()
}

// Load returns the last payload and its timestamp.
// ok=false if no snapshot has been received yet.
func (rs *RobotState) Load() (payload []byte, receivedAt time.Time, ok bool) {
	rs.mu.RLock()
	defer rs.mu.RUnlock()
	if rs.receivedAt.IsZero() {
		return nil, time.Time{}, false
	}
	return rs.lastPayload, rs.receivedAt, true
}

// ─── estopHandler ─────────────────────────────────────────────────────────────
//
// POST /api/robot/estop
//
// Publishes {"source":"...","timestamp":...} to robot/commands/estop at QoS 2.
// Returns:
//
//	200 — published successfully (robot should stop)
//	502 — MQTT broker unreachable (robot may NOT stop — warn operator)
//	500 — unexpected error
func (s *Server) estopHandler(mqttClient *mqtt.Client) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			writeJSON(w, http.StatusMethodNotAllowed,
				errorResponse{Error: "method not allowed"})
			return
		}

		// Decode optional body (source, timestamp fields from Flutter).
		// We allow an empty body — not required for the command to work.
		var body struct {
			Source    string `json:"source"`
			Timestamp int64  `json:"timestamp"`
		}
		_ = json.NewDecoder(r.Body).Decode(&body)

		if body.Source == "" {
			body.Source = "operator_app"
		}
		if body.Timestamp == 0 {
			body.Timestamp = time.Now().UnixMilli()
		}

		payload, err := json.Marshal(map[string]any{
			"source":    body.Source,
			"timestamp": body.Timestamp,
		})
		if err != nil {
			s.log.Error("estop: failed to marshal payload", "error", err)
			writeJSON(w, http.StatusInternalServerError,
				errorResponse{Error: "internal server error"})
			return
		}

		// Publish at QoS 2 — mqtt.Client.PublishQoS2 blocks until the
		// PUBCOMP handshake completes or times out (5 s).
		if err := mqttClient.PublishQoS2(mqtt.TopicEstop, payload); err != nil {
			s.log.Error("estop: MQTT publish failed",
				"error", err,
				"source", body.Source,
			)
			writeJSON(w, http.StatusBadGateway,
				errorResponse{Error: fmt.Sprintf("MQTT broker unreachable: %v", err)})
			return
		}

		s.log.Warn("ESTOP published",
			"source", body.Source,
			"timestamp", body.Timestamp,
		)
		writeJSON(w, http.StatusOK, map[string]any{
			"sent":      true,
			"topic":     mqtt.TopicEstop,
			"source":    body.Source,
			"timestamp": body.Timestamp,
		})
	}
}

// ─── telemetryHandler ─────────────────────────────────────────────────────────
//
// GET /api/robot/telemetry
//
// Returns the last telemetry snapshot received on robot/telemetry.
// Returns:
//
//	200 — snapshot available (JSON body)
//	204 — no snapshot received since gateway startup
func (s *Server) telemetryHandler(robotState *RobotState) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			writeJSON(w, http.StatusMethodNotAllowed,
				errorResponse{Error: "method not allowed"})
			return
		}

		payload, receivedAt, ok := robotState.Load()
		if !ok {
			w.WriteHeader(http.StatusNoContent)
			return
		}

		// Inject a received_at field so the Flutter client knows the age
		// of the snapshot without needing to compare local timestamps.
		var raw map[string]any
		if err := json.Unmarshal(payload, &raw); err != nil {
			s.log.Error("telemetry: failed to unmarshal stored payload", "error", err)
			writeJSON(w, http.StatusInternalServerError,
				errorResponse{Error: "internal server error"})
			return
		}
		raw["received_at"] = receivedAt.UTC().UnixMilli()

		w.Header().Set("Content-Type", "application/json")
		w.WriteHeader(http.StatusOK)
		_ = json.NewEncoder(w).Encode(raw)
	}
}

type cameraConfigResponse struct {
	Available          bool     `json:"available"`
	StreamURL          string   `json:"stream_url"`
	StreamKind         string   `json:"stream_kind"`
	SignalingURL       string   `json:"signaling_url"`
	ICEServers         []string `json:"ice_servers"`
	Label              string   `json:"label"`
	LatencyTargetMS    int      `json:"latency_target_ms"`
	ROSImageTopic      string   `json:"ros_image_topic"`
	ROSCompressedTopic string   `json:"ros_compressed_topic"`
}

// GET /api/robot/camera
//
// Returns the operator camera stream metadata. The gateway does not proxy video
// in Phase 1; it only tells the app which stream endpoint to render.
func (s *Server) cameraConfigHandler() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodGet {
			writeJSON(w, http.StatusMethodNotAllowed,
				errorResponse{Error: "method not allowed"})
			return
		}

		streamURL := os.Getenv("ROBOT_CAMERA_STREAM_URL")
		streamKind := os.Getenv("ROBOT_CAMERA_STREAM_KIND")
		if streamKind == "" {
			streamKind = "unset"
		}
		signalingURL := os.Getenv("ROBOT_CAMERA_SIGNALING_URL")
		iceServers := splitCSVEnv("ROBOT_CAMERA_ICE_SERVERS")
		label := os.Getenv("ROBOT_CAMERA_LABEL")
		if label == "" {
			label = "C920 front camera"
		}
		latencyTargetMS := 500
		if raw := os.Getenv("ROBOT_CAMERA_LATENCY_TARGET_MS"); raw != "" {
			if parsed, err := strconv.Atoi(raw); err == nil && parsed > 0 {
				latencyTargetMS = parsed
			}
		}
		rosImageTopic := os.Getenv("ROBOT_CAMERA_ROS_IMAGE_TOPIC")
		if rosImageTopic == "" {
			rosImageTopic = "/camera/image_raw"
		}
		rosCompressedTopic := os.Getenv("ROBOT_CAMERA_ROS_COMPRESSED_TOPIC")
		if rosCompressedTopic == "" {
			rosCompressedTopic = "/camera/image_raw/compressed"
		}

		writeJSON(w, http.StatusOK, cameraConfigResponse{
			Available:          streamURL != "",
			StreamURL:          streamURL,
			StreamKind:         streamKind,
			SignalingURL:       signalingURL,
			ICEServers:         iceServers,
			Label:              label,
			LatencyTargetMS:    latencyTargetMS,
			ROSImageTopic:      rosImageTopic,
			ROSCompressedTopic: rosCompressedTopic,
		})
	}
}

func splitCSVEnv(key string) []string {
	raw := os.Getenv(key)
	if raw == "" {
		return nil
	}
	parts := strings.Split(raw, ",")
	values := make([]string, 0, len(parts))
	for _, part := range parts {
		value := strings.TrimSpace(part)
		if value != "" {
			values = append(values, value)
		}
	}
	return values
}
