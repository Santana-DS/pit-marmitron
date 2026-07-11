// internal/api/server.go
//
// CHANGES IN THIS REVISION (Phase 1):
//   - GET /api/waypoints registered via listWaypointsHandler().
//     All other routes and wiring are unchanged.
package api

import (
	"context"
	"encoding/json"
	"log/slog"
	"net/http"
	"time"

	"unbot-gateway/internal/catalog"
	"unbot-gateway/internal/database"
	"unbot-gateway/internal/mqtt"
	"unbot-gateway/internal/orders"
	"unbot-gateway/internal/services"
)

type Server struct {
	addr   string
	log    *slog.Logger
	mux    *http.ServeMux
	server *http.Server
}

func NewServer(
	addr string,
	log *slog.Logger,
	otpSvc *services.OTPService,
	orderSvc *services.OrderService,
	wakeSvc *services.WakeDisplayService,
	catalogSvc *catalog.Service,
	ordersSvc *orders.Service,
	mqttClient *mqtt.Client,
	robotState *RobotState,
	telemetryRepo *database.TelemetryRepository,
	deliveryPoints *database.DeliveryPointRepository,
) *Server {
	s := &Server{
		addr: addr,
		log:  log,
		mux:  http.NewServeMux(),
	}
	s.routes(otpSvc, orderSvc, wakeSvc, catalogSvc, ordersSvc, mqttClient, robotState, telemetryRepo, deliveryPoints)
	s.server = &http.Server{
		Addr:         addr,
		Handler:      s.corsMiddleware(s.mux),
		ReadTimeout:  10 * time.Second,
		WriteTimeout: 10 * time.Second,
		IdleTimeout:  60 * time.Second,
	}
	return s
}

func (s *Server) Start() {
	go func() {
		s.log.Info("HTTP server listening", "addr", s.addr)
		if err := s.server.ListenAndServe(); err != nil && err != http.ErrServerClosed {
			s.log.Error("HTTP server error", "error", err)
		}
	}()
}

func (s *Server) Shutdown(ctx context.Context) error {
	s.log.Info("shutting down HTTP server")
	return s.server.Shutdown(ctx)
}

func (s *Server) corsMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		origin := r.Header.Get("Origin")
		if origin != "" {
			w.Header().Set("Access-Control-Allow-Origin", origin)
		} else {
			w.Header().Set("Access-Control-Allow-Origin", "*")
		}
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, PATCH, DELETE, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With")
		w.Header().Set("Access-Control-Allow-Credentials", "true")
		w.Header().Set("Access-Control-Max-Age", "3600")
		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusNoContent)
			return
		}
		next.ServeHTTP(w, r)
	})
}

func (s *Server) routes(
	otpSvc *services.OTPService,
	orderSvc *services.OrderService,
	wakeSvc *services.WakeDisplayService,
	catalogSvc *catalog.Service,
	ordersSvc *orders.Service,
	mqttClient *mqtt.Client,
	robotState *RobotState,
	telemetryRepo telemetryHistoryReader,
	deliveryPoints deliveryPointReader,
) {
	s.mux.HandleFunc("GET /health", s.handleHealth)
	s.mux.HandleFunc("POST /api/validate-code", s.validateCodeHandler(otpSvc))
	s.mux.HandleFunc("POST /api/orders/{id}/dispatch", s.dispatchHandler(orderSvc))
	s.mux.HandleFunc("POST /api/orders/{id}/wake-display", s.wakeDisplayHandler(wakeSvc))

	if deliveryPoints != nil {
		s.mux.HandleFunc("GET /api/delivery-points", s.listDeliveryPointsHandler(deliveryPoints))
	}

	// Operator endpoints
	if mqttClient != nil {
		s.mux.HandleFunc("POST /api/robot/estop", s.estopHandler(mqttClient))
	}
	s.mux.HandleFunc("GET /api/robot/telemetry", s.telemetryHandler(robotState))
	s.mux.HandleFunc("GET /api/robot/camera", s.cameraConfigHandler())
	if telemetryRepo != nil {
		s.mux.HandleFunc(
			"GET /api/operator/deliveries/{order_id}/telemetry",
			s.operatorTelemetryHistoryHandler(telemetryRepo),
		)
	}

	if catalogSvc != nil {
		s.mux.HandleFunc("GET /api/restaurants", s.listRestaurantsHandler(catalogSvc))
		s.mux.HandleFunc("GET /api/restaurants/{id}", s.getRestaurantHandler(catalogSvc))
		s.mux.HandleFunc("GET /api/restaurants/{id}/products", s.listRestaurantProductsHandler(catalogSvc))
		s.mux.HandleFunc("GET /api/restaurants/{id}/orders", s.listOrdersByRestaurantHandler(ordersSvc))
	}

	if ordersSvc != nil {
		s.mux.HandleFunc("POST /api/orders", s.createOrderHandler(ordersSvc))
		s.mux.HandleFunc("GET /api/orders/{id}", s.getOrderHandler(ordersSvc))
		s.mux.HandleFunc("GET /api/clients/{client_id}/orders", s.listOrdersByClientHandler(ordersSvc))
		s.mux.HandleFunc("PATCH /api/orders/{id}/status", s.updateOrderStatusHandler(ordersSvc))
	}
}

type healthResponse struct {
	Status  string `json:"status"`
	Version string `json:"version"`
}

func (s *Server) handleHealth(w http.ResponseWriter, r *http.Request) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	_ = json.NewEncoder(w).Encode(healthResponse{
		Status:  "ok",
		Version: "3.0.0", // bumped for Phase 1
	})
}
