// internal/services/order.go
//
// CHANGES IN THIS REVISION (Phase 1 — Live Navigation)
// ──────────────────────────────────────────────────────
// EXTENDED: NavigatePayload now carries map_frame and theta so Nav2 can
//
//	receive a full PoseStamped. Previous payload only had {x, y} which is
//	insufficient for Nav2's /navigate_to_pose action server — it requires
//	orientation (yaw) to determine the robot's heading at the goal.
//
// ADDED: WaypointRegistry — a compile-time map of named campus locations to
//
//	ROS 2 map-frame coordinates. The Go backend no longer hardcodes
//	{x: 12.0, y: -3.5}. The Flutter client sends a named destination string
//	(e.g. "FT_ENTRADA"), and Dispatch() resolves it to a full Pose.
//	This registry will be replaced by a database table in Phase 2, but the
//	interface (LookupWaypoint) is already abstracted so the swap is local.
//
// CHANGED: Destination struct now includes MapFrame and Theta.
//
//	Callers (dispatch.go) that previously supplied {X, Y} must now also
//	supply the waypoint name and let Dispatch resolve it, OR supply all
//	fields explicitly. dispatch.go is updated in this same revision to
//	accept a waypoint_name field in the HTTP body.
//
// UNCHANGED: WakeDisplayService, OTPService wiring, GatewayMode constants.
package services

import (
	"encoding/json"
	"fmt"
	"log/slog"
	"math"
	"time"
)

// ── Gateway mode constants ────────────────────────────────────────────────────

const (
	GatewayModeFull    = "full"
	GatewayModeOTPOnly = "otp_only"
)

// ── WaypointRegistry ─────────────────────────────────────────────────────────
//
// Named campus locations in the ROS 2 map frame ("map").
// Coordinates are in metres, origin at SLAM map origin (set during map
// recording session). Theta is in radians, measured CCW from +X axis.
//
// HOW TO CALIBRATE:
//   1. Drive robot to the physical location.
//   2. Run: ros2 topic echo /amcl_pose --once
//   3. Copy pose.position.x, pose.position.y and convert quaternion to yaw.
//   4. Add entry here and commit.
//
// COORDINATE SYSTEM NOTE:
//   These are ROS map-frame coordinates, NOT GPS. If your map origin shifts
//   after re-running SLAM, all coordinates must be re-surveyed.
//   Consider pinning your map with known anchor points (see Nav2 docs on
//   map_server with --ros-args remapping).

type Waypoint struct {
	// Name is the human-readable campus location identifier.
	// Used in logs and MQTT payloads for traceability.
	Name string

	// X, Y are map-frame coordinates in metres.
	X float64
	Y float64

	// Theta is the goal heading in radians (CCW from +X axis).
	// 0.0  = facing right on the map (+X direction)
	// π/2  = facing up (+Y direction)
	// π    = facing left (-X direction)
	// -π/2 = facing down (-Y direction)
	Theta float64

	// MapFrame is the ROS 2 TF frame for this waypoint.
	// Almost always "map" — only differs if using a multi-floor setup.
	MapFrame string
}

// waypointRegistry is the campus waypoint lookup table.
// Keys are stable string identifiers used by the Flutter client and
// Go dispatch handler. Never change a key after it has been stored in an
// active order — use a new key and keep the old one as a deprecated alias.
//
// ── How to derive Theta from a quaternion ────────────────────────────────────
// If /amcl_pose gives you a quaternion (qx, qy, qz, qw):
//   yaw = 2 * atan2(qz, qw)
// Standard Go math: theta = 2 * math.Atan2(qz, qw)

var waypointRegistry = map[string]Waypoint{
	// ── Faculdade de Tecnologia ───────────────────────────────────────────
	"FT_ENTRADA": {
		Name:     "FT — Entrada principal",
		X:        12.0, // PLACEHOLDER: replace after SLAM survey
		Y:        -3.5, // PLACEHOLDER: replace after SLAM survey
		Theta:    0.0,  // PLACEHOLDER: facing +X (east)
		MapFrame: "map",
	},
	"FT_CORREDOR_A": {
		Name:     "FT — Corredor A (térreo)",
		X:        18.4,        // PLACEHOLDER
		Y:        2.1,         // PLACEHOLDER
		Theta:    math.Pi / 2, // facing north
		MapFrame: "map",
	},
	"FT_CORREDOR_B": {
		Name:     "FT — Corredor B (térreo)",
		X:        18.4,         // PLACEHOLDER
		Y:        -8.2,         // PLACEHOLDER
		Theta:    -math.Pi / 2, // facing south
		MapFrame: "map",
	},
	"FT_LABORATORIO_1": {
		Name:     "FT — Laboratório 1",
		X:        24.0,    // PLACEHOLDER
		Y:        5.0,     // PLACEHOLDER
		Theta:    math.Pi, // facing west (back to corridor)
		MapFrame: "map",
	},
	// ── Charging station ─────────────────────────────────────────────────
	// Robot returns here after delivery completion or low battery.
	"BASE_CHARGING": {
		Name:     "Base — Estação de carregamento",
		X:        0.5, // near map origin
		Y:        0.5,
		Theta:    math.Pi, // docking orientation
		MapFrame: "map",
	},
}

// LookupWaypoint resolves a named waypoint to its map coordinates.
// Returns an error if the key is unknown — callers must handle this
// before constructing a NavigatePayload.
func LookupWaypoint(name string) (Waypoint, error) {
	wp, ok := waypointRegistry[name]
	if !ok {
		return Waypoint{}, fmt.Errorf("unknown waypoint %q: not in registry", name)
	}
	return wp, nil
}

// ListWaypoints returns all registered waypoint names.
// Used by the GET /api/waypoints endpoint so Flutter can populate a
// destination picker without hardcoding names.
func ListWaypoints() []string {
	names := make([]string, 0, len(waypointRegistry))
	for k := range waypointRegistry {
		names = append(names, k)
	}
	return names
}

// ── Destination (extended) ────────────────────────────────────────────────────
//
// BREAKING CHANGE from Phase 0: added MapFrame and Theta.
// dispatch.go is updated in this revision to populate these fields.
// The old {X, Y} only struct is no longer accepted — dispatch.go validates
// that WaypointName is provided and resolves it via LookupWaypoint().

type Destination struct {
	// WaypointName is the registry key, e.g. "FT_ENTRADA".
	// If non-empty, Dispatch() resolves X/Y/Theta/MapFrame from the registry.
	// If empty, X/Y/Theta/MapFrame must be provided directly (internal use).
	WaypointName string `json:"waypoint_name,omitempty"`

	X        float64 `json:"x"`
	Y        float64 `json:"y"`
	Theta    float64 `json:"theta"`
	MapFrame string  `json:"map_frame"`
}

// ── Payload types ─────────────────────────────────────────────────────────────

// NavigatePayload is published to robot/commands/navigate.
//
// MQTT payload (extended) received by the edge daemon:
//
//	{
//	  "order_id":   "order_1714000000123",
//	  "map_frame":  "map",
//	  "pose": {
//	    "x":     12.0,
//	    "y":     -3.5,
//	    "theta": 0.0
//	  },
//	  "waypoint_name": "FT_ENTRADA",
//	  "issued_at": 1714000000
//	}
//
// The edge daemon extracts pose and map_frame to build a ROS 2 PoseStamped.
// waypoint_name is included for logging on the daemon side.
type NavigatePayload struct {
	OrderID      string  `json:"order_id"`
	MapFrame     string  `json:"map_frame"`
	Pose         NavPose `json:"pose"`
	WaypointName string  `json:"waypoint_name"`
	IssuedAt     int64   `json:"issued_at"`
}

// NavPose is the 2D pose component of NavigatePayload.
// Theta is yaw in radians.
type NavPose struct {
	X     float64 `json:"x"`
	Y     float64 `json:"y"`
	Theta float64 `json:"theta"`
}

// DisplayQRPayload is unchanged.
type DisplayQRPayload struct {
	OrderID  string `json:"order_id"`
	OTP      string `json:"otp"`
	IssuedAt int64  `json:"issued_at"`
}

// ── DispatchResult ────────────────────────────────────────────────────────────

type DispatchResult struct {
	Success       bool   `json:"success"`
	OrderID       string `json:"order_id"`
	Status        string `json:"status"`
	OTPCode       string `json:"otp_code"`
	MQTTConnected bool   `json:"mqtt_connected"`
	GatewayMode   string `json:"gateway_mode"`
	// WaypointName echoed back so Flutter can display the destination name.
	WaypointName string `json:"waypoint_name,omitempty"`
}

// ── Errors ────────────────────────────────────────────────────────────────────

var (
	ErrOTPIssuance     = fmt.Errorf("failed to issue OTP for order")
	ErrWakeDisplay     = fmt.Errorf("display wake command could not be delivered")
	ErrUnknownWaypoint = fmt.Errorf("destination waypoint not in registry")
)

// =============================================================================
// OrderService
// =============================================================================

type OrderService struct {
	otpSvc    *OTPService
	publisher Publisher
	log       *slog.Logger
}

func NewOrderService(otpSvc *OTPService, publisher Publisher, log *slog.Logger) *OrderService {
	return &OrderService{
		otpSvc:    otpSvc,
		publisher: publisher,
		log:       log,
	}
}

// Dispatch issues an OTP and publishes the navigate command with full pose.
//
// DESTINATION RESOLUTION:
//
//	If dest.WaypointName is non-empty, the registry is consulted and
//	dest.X/Y/Theta/MapFrame are populated from the registry entry.
//	This is the normal production path (Flutter sends a waypoint name).
//
//	If dest.WaypointName is empty, X/Y/Theta/MapFrame must already be set
//	(used in tests or when the caller has pre-resolved coordinates).
func (s *OrderService) Dispatch(orderID string, dest Destination) (*DispatchResult, error) {
	// ── Step 0: Resolve waypoint if named ────────────────────────────────
	resolvedWaypointName := dest.WaypointName
	if dest.WaypointName != "" {
		wp, err := LookupWaypoint(dest.WaypointName)
		if err != nil {
			s.log.Error("waypoint lookup failed",
				"order_id", orderID,
				"waypoint", dest.WaypointName,
				"error", err,
			)
			return nil, fmt.Errorf("%w: %v", ErrUnknownWaypoint, err)
		}
		dest.X = wp.X
		dest.Y = wp.Y
		dest.Theta = wp.Theta
		dest.MapFrame = wp.MapFrame
		s.log.Info("waypoint resolved",
			"order_id", orderID,
			"waypoint", dest.WaypointName,
			"x", dest.X,
			"y", dest.Y,
			"theta", dest.Theta,
		)
	}

	// Default map_frame if caller omitted it (backwards-compat for direct coords).
	if dest.MapFrame == "" {
		dest.MapFrame = "map"
	}

	// ── Step 1: Issue OTP ─────────────────────────────────────────────────
	otpCode, err := s.otpSvc.IssueOTP(orderID)
	if err != nil {
		s.log.Error("OTP issuance failed", "order_id", orderID, "error", err)
		return nil, ErrOTPIssuance
	}
	s.log.Info("OTP issued", "order_id", orderID)

	// ── Step 2: Publish navigate command ──────────────────────────────────
	mqttConnected := true
	gatewayMode := GatewayModeFull

	navPayload, marshalErr := json.Marshal(NavigatePayload{
		OrderID:  orderID,
		MapFrame: dest.MapFrame,
		Pose: NavPose{
			X:     dest.X,
			Y:     dest.Y,
			Theta: dest.Theta,
		},
		WaypointName: resolvedWaypointName,
		IssuedAt:     time.Now().Unix(),
	})
	if marshalErr != nil {
		s.log.Error("navigate payload marshal failed", "order_id", orderID, "error", marshalErr)
		mqttConnected = false
		gatewayMode = GatewayModeOTPOnly
	}

	if mqttConnected {
		if pubErr := s.publisher.Publish(TopicNavigate, navPayload); pubErr != nil {
			s.log.Warn("navigate MQTT publish failed — degrading to otp_only",
				"order_id", orderID, "error", pubErr)
			mqttConnected = false
			gatewayMode = GatewayModeOTPOnly
		} else {
			s.log.Info("navigate command published",
				"order_id", orderID,
				"topic", TopicNavigate,
				"waypoint", resolvedWaypointName,
				"destination_x", dest.X,
				"destination_y", dest.Y,
				"theta", dest.Theta,
				"map_frame", dest.MapFrame,
			)
		}
	}

	// ── Step 3: Return result ─────────────────────────────────────────────
	return &DispatchResult{
		Success:       true,
		OrderID:       orderID,
		Status:        gatewayMode,
		OTPCode:       otpCode,
		MQTTConnected: mqttConnected,
		GatewayMode:   gatewayMode,
		WaypointName:  resolvedWaypointName,
	}, nil
}

// =============================================================================
// WakeDisplayService — unchanged from Phase 0
// =============================================================================

type WakeDisplayService struct {
	otpSvc    *OTPService
	publisher Publisher
	log       *slog.Logger
}

func NewWakeDisplayService(otpSvc *OTPService, publisher Publisher, log *slog.Logger) *WakeDisplayService {
	return &WakeDisplayService{
		otpSvc:    otpSvc,
		publisher: publisher,
		log:       log,
	}
}

func (s *WakeDisplayService) WakeDisplay(orderID string) error {
	otp, err := s.otpSvc.LookupByOrderID(orderID)
	if err != nil {
		s.log.Warn("wake-display: order not found or OTP consumed",
			"order_id", orderID, "error", err)
		return fmt.Errorf("%w: %v", ErrOrderNotFound, err)
	}

	payload, marshalErr := json.Marshal(DisplayQRPayload{
		OrderID:  orderID,
		OTP:      otp,
		IssuedAt: time.Now().Unix(),
	})
	if marshalErr != nil {
		return fmt.Errorf("%w: marshal: %v", ErrWakeDisplay, marshalErr)
	}

	if pubErr := s.publisher.Publish(TopicDisplayQR, payload); pubErr != nil {
		s.log.Error("wake-display: MQTT publish failed",
			"order_id", orderID, "error", pubErr)
		return fmt.Errorf("%w: %v", ErrWakeDisplay, pubErr)
	}

	s.log.Info("wake-display: display_qr published",
		"order_id", orderID, "topic", TopicDisplayQR)
	return nil
}
