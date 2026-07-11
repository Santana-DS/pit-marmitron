package api

import (
	"context"
	"net/http"
	"time"

	"unbot-gateway/internal/database"
)

type telemetryHistoryReader interface {
	ListByOrder(context.Context, string, *time.Time) ([]database.TelemetryHistoryPoint, error)
}

type operatorPoseResponse struct {
	X     *float64 `json:"x"`
	Y     *float64 `json:"y"`
	Theta *float64 `json:"theta"`
	Frame *string  `json:"frame"`
}

type operatorTelemetryPointResponse struct {
	Timestamp      time.Time             `json:"timestamp"`
	Pose           *operatorPoseResponse `json:"pose"`
	BatteryPercent *float64              `json:"battery_percent"`
	NavState       string                `json:"nav_state"`
	RemainingM     *float64              `json:"remaining_m"`
	ProgressPct    *float64              `json:"progress_pct"`
}

// GET /api/operator/deliveries/{order_id}/telemetry?from=<RFC3339>
//
// The endpoint reads the durable history rather than RobotState so a map can
// be replayed after an app refresh or a temporary gateway disconnect.
func (s *Server) operatorTelemetryHistoryHandler(repo telemetryHistoryReader) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		orderID := r.PathValue("order_id")
		if orderID == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "order_id is required"})
			return
		}

		var from *time.Time
		if rawFrom := r.URL.Query().Get("from"); rawFrom != "" {
			parsed, err := time.Parse(time.RFC3339, rawFrom)
			if err != nil {
				writeJSON(w, http.StatusBadRequest, errorResponse{Error: "from must be RFC3339"})
				return
			}
			from = &parsed
		}

		points, err := repo.ListByOrder(r.Context(), orderID, from)
		if err != nil {
			s.log.Error("operator telemetry history query failed", "error", err, "order_id", orderID)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		response := make([]operatorTelemetryPointResponse, 0, len(points))
		for _, point := range points {
			var pose *operatorPoseResponse
			if point.PoseX != nil && point.PoseY != nil && point.PoseTheta != nil {
				pose = &operatorPoseResponse{
					X: point.PoseX, Y: point.PoseY, Theta: point.PoseTheta, Frame: point.MapFrame,
				}
			}
			response = append(response, operatorTelemetryPointResponse{
				Timestamp: point.Timestamp, Pose: pose, BatteryPercent: point.BatteryPercent,
				NavState: point.NavState, RemainingM: point.RemainingM, ProgressPct: point.ProgressPct,
			})
		}

		writeJSON(w, http.StatusOK, map[string]any{
			"order_id": orderID,
			"points":   response,
		})
	}
}
