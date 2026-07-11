package api

import (
	"context"
	"net/http"

	"unbot-gateway/internal/database"
)

type deliveryPointReader interface {
	ListActive(context.Context) ([]database.DeliveryPoint, error)
}

type deliveryPointsResponse struct {
	DeliveryPoints []deliveryPointResponse `json:"delivery_points"`
}

type deliveryPointResponse struct {
	PointKey       string  `json:"point_key"`
	Label          string  `json:"label"`
	DisplayAddress string  `json:"display_address"`
	Latitude       float64 `json:"latitude"`
	Longitude      float64 `json:"longitude"`
}

// GET /api/delivery-points exposes map-display coordinates and labels. ROS
// coordinates are intentionally not included in the mobile response.
func (s *Server) listDeliveryPointsHandler(repo deliveryPointReader) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		points, err := repo.ListActive(r.Context())
		if err != nil {
			s.log.Error("list delivery points failed", "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		response := make([]deliveryPointResponse, 0, len(points))
		for _, point := range points {
			response = append(response, deliveryPointResponse{
				PointKey: point.PointKey, Label: point.Label,
				DisplayAddress: point.DisplayAddress,
				Latitude:       point.Latitude, Longitude: point.Longitude,
			})
		}
		writeJSON(w, http.StatusOK, deliveryPointsResponse{DeliveryPoints: response})
	}
}
