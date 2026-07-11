package api

import (
	"context"
	"encoding/json"
	"errors"
	"log/slog"
	"net/http"
	"net/http/httptest"
	"testing"
	"time"

	"unbot-gateway/internal/database"
)

type telemetryHistoryRepoStub struct {
	points []database.TelemetryHistoryPoint
	err    error
	order  string
	from   *time.Time
}

func (r *telemetryHistoryRepoStub) ListByOrder(
	_ context.Context,
	orderID string,
	from *time.Time,
) ([]database.TelemetryHistoryPoint, error) {
	r.order = orderID
	r.from = from
	return r.points, r.err
}

func TestOperatorTelemetryHistoryHandler(t *testing.T) {
	x, y, theta := 12.35, -3.42, 1.5708
	frame := "map"
	battery := 67.5
	repo := &telemetryHistoryRepoStub{points: []database.TelemetryHistoryPoint{
		{
			Timestamp: time.Date(2026, 7, 11, 15, 0, 0, 0, time.UTC),
			PoseX:     &x, PoseY: &y, PoseTheta: &theta, MapFrame: &frame,
			BatteryPercent: &battery, NavState: "NAVIGATING",
		},
		{Timestamp: time.Date(2026, 7, 11, 15, 0, 2, 0, time.UTC), NavState: "IDLE"},
	}}
	s := &Server{log: slog.Default(), mux: http.NewServeMux()}
	s.mux.HandleFunc(
		"GET /api/operator/deliveries/{order_id}/telemetry",
		s.operatorTelemetryHistoryHandler(repo),
	)

	req := httptest.NewRequest(
		http.MethodGet,
		"/api/operator/deliveries/order-42/telemetry?from=2026-07-11T14:00:00Z",
		nil,
	)
	rr := httptest.NewRecorder()
	s.mux.ServeHTTP(rr, req)

	if rr.Code != http.StatusOK {
		t.Fatalf("status = %d, want %d", rr.Code, http.StatusOK)
	}
	if repo.order != "order-42" {
		t.Fatalf("order = %q, want order-42", repo.order)
	}
	if repo.from == nil || repo.from.Format(time.RFC3339) != "2026-07-11T14:00:00Z" {
		t.Fatalf("from = %v, want parsed RFC3339 timestamp", repo.from)
	}

	var response struct {
		OrderID string                           `json:"order_id"`
		Points  []operatorTelemetryPointResponse `json:"points"`
	}
	if err := json.NewDecoder(rr.Body).Decode(&response); err != nil {
		t.Fatalf("decode response: %v", err)
	}
	if response.OrderID != "order-42" || len(response.Points) != 2 {
		t.Fatalf("response = %+v, want order and two points", response)
	}
	if response.Points[0].Pose == nil || *response.Points[0].Pose.X != x {
		t.Fatalf("first pose = %+v, want x %v", response.Points[0].Pose, x)
	}
	if response.Points[1].Pose != nil {
		t.Fatalf("second pose = %+v, want nil", response.Points[1].Pose)
	}
}

func TestOperatorTelemetryHistoryHandlerRejectsInvalidFrom(t *testing.T) {
	repo := &telemetryHistoryRepoStub{}
	s := &Server{log: slog.Default(), mux: http.NewServeMux()}
	s.mux.HandleFunc(
		"GET /api/operator/deliveries/{order_id}/telemetry",
		s.operatorTelemetryHistoryHandler(repo),
	)

	req := httptest.NewRequest(
		http.MethodGet,
		"/api/operator/deliveries/order-42/telemetry?from=not-a-date",
		nil,
	)
	rr := httptest.NewRecorder()
	s.mux.ServeHTTP(rr, req)

	if rr.Code != http.StatusBadRequest {
		t.Fatalf("status = %d, want %d", rr.Code, http.StatusBadRequest)
	}
	if repo.order != "" {
		t.Fatalf("repository called for invalid timestamp: %q", repo.order)
	}
}

func TestOperatorTelemetryHistoryHandlerReturnsServerError(t *testing.T) {
	repo := &telemetryHistoryRepoStub{err: errors.New("database unavailable")}
	s := &Server{log: slog.Default(), mux: http.NewServeMux()}
	s.mux.HandleFunc(
		"GET /api/operator/deliveries/{order_id}/telemetry",
		s.operatorTelemetryHistoryHandler(repo),
	)

	req := httptest.NewRequest(http.MethodGet, "/api/operator/deliveries/order-42/telemetry", nil)
	rr := httptest.NewRecorder()
	s.mux.ServeHTTP(rr, req)

	if rr.Code != http.StatusInternalServerError {
		t.Fatalf("status = %d, want %d", rr.Code, http.StatusInternalServerError)
	}
}
