// internal/api/dispatch.go
//
// CHANGES IN THIS REVISION (Phase 1 — Live Navigation)
// ──────────────────────────────────────────────────────
// CHANGED: dispatchRequest now accepts waypoint_name (preferred) as an
//
//	alternative to raw x/y coordinates. If waypoint_name is provided,
//	x/y coordinate validation is skipped — the registry lookup in
//	OrderService.Dispatch() performs its own validation.
//
// ADDED: GET /api/waypoints handler registered in server.go (new endpoint).
//
//	Returns the list of named waypoints so Flutter can populate a picker.
//
// UNCHANGED: NaN/Inf coordinate validation, error mapping, writeJSON helper.
package api

import (
	"encoding/json"
	"errors"
	"math"
	"net/http"
	"strings"

	"unbot-gateway/internal/services"
)

// ── Request / Response types ──────────────────────────────────────────────────

type destinationRequest struct {
	X float64 `json:"x"`
	Y float64 `json:"y"`
}

// dispatchRequest is extended with waypoint_name.
//
// Two valid calling modes:
//
//	MODE A — named waypoint (preferred, used by Flutter production app):
//	  { "waypoint_name": "FT_ENTRADA", "restaurant_name": "Marmitas da Vó" }
//	  Coordinates are resolved from the registry. x/y/destination ignored.
//
//	MODE B — explicit coordinates (used by tests and legacy callers):
//	  { "destination": {"x": 12.0, "y": -3.5}, "restaurant_name": "..." }
//	  waypoint_name is empty. x/y are validated for NaN/Inf.
//	  Theta defaults to 0.0, map_frame defaults to "map".
type dispatchRequest struct {
	Destination    destinationRequest `json:"destination"`
	RestaurantName string             `json:"restaurant_name"`
	// WaypointName is the preferred way to specify a destination.
	// When non-empty, Destination is ignored.
	WaypointName string `json:"waypoint_name"`
}

// ── Handler ───────────────────────────────────────────────────────────────────

func (s *Server) dispatchHandler(orderSvc *services.OrderService) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		if r.Method != http.MethodPost {
			writeJSON(w, http.StatusMethodNotAllowed,
				errorResponse{Error: "method not allowed"})
			return
		}

		orderID := strings.TrimSpace(r.PathValue("id"))
		if orderID == "" {
			writeJSON(w, http.StatusBadRequest,
				errorResponse{Error: "order_id path parameter is required"})
			return
		}

		var req dispatchRequest
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			writeJSON(w, http.StatusBadRequest,
				errorResponse{Error: "malformed JSON body"})
			return
		}

		// ── Build Destination from request ────────────────────────────────
		if req.WaypointName == "" {
			writeJSON(w, http.StatusBadRequest,
				errorResponse{Error: "a calibrated delivery point is required"})
			return
		}

		var dest services.Destination

		if req.WaypointName != "" {
			// MODE A: named waypoint — let the service layer resolve coordinates.
			dest.WaypointName = req.WaypointName
		} else {
			// MODE B: explicit coordinates — validate for NaN/Inf.
			if !isFiniteCoord(req.Destination.X) || !isFiniteCoord(req.Destination.Y) {
				writeJSON(w, http.StatusBadRequest,
					errorResponse{Error: "destination coordinates must be finite numbers"})
				return
			}
			dest.X = req.Destination.X
			dest.Y = req.Destination.Y
			dest.Theta = 0.0      // default heading
			dest.MapFrame = "map" // default frame
		}

		// ── Delegate to service ───────────────────────────────────────────
		result, err := orderSvc.Dispatch(r.Context(), orderID, dest)
		if err != nil {
			switch {
			case errors.Is(err, services.ErrUnknownWaypoint):
				s.log.Warn("dispatch rejected: unknown waypoint",
					"order_id", orderID,
					"waypoint", req.WaypointName,
				)
				writeJSON(w, http.StatusBadRequest,
					errorResponse{Error: "unknown destination waypoint: " + req.WaypointName})
			case errors.Is(err, services.ErrOTPIssuance):
				s.log.Error("OTP issuance failed in dispatch",
					"order_id", orderID,
					"error", err,
				)
				writeJSON(w, http.StatusInternalServerError,
					errorResponse{Error: "failed to generate order code; please retry"})
			default:
				s.log.Error("unexpected error in Dispatch",
					"order_id", orderID,
					"error", err,
				)
				writeJSON(w, http.StatusInternalServerError,
					errorResponse{Error: "internal server error"})
			}
			return
		}

		s.log.Info("order dispatched",
			"order_id", orderID,
			"restaurant", req.RestaurantName,
			"waypoint", result.WaypointName,
			"gateway_mode", result.GatewayMode,
			"mqtt_connected", result.MQTTConnected,
		)

		writeJSON(w, http.StatusOK, result)
	}
}

// ── Waypoints handler ─────────────────────────────────────────────────────────
//
// GET /api/waypoints
// Returns the list of named delivery destinations so Flutter can populate
// a destination picker without hardcoding waypoint keys.

type waypointsResponse struct {
	Waypoints []string `json:"waypoints"`
}

func (s *Server) listWaypointsHandler() http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		writeJSON(w, http.StatusOK, waypointsResponse{
			Waypoints: services.ListWaypoints(),
		})
	}
}

// ── Helpers ───────────────────────────────────────────────────────────────────

func isFiniteCoord(f float64) bool {
	return !math.IsNaN(f) && !math.IsInf(f, 0)
}
