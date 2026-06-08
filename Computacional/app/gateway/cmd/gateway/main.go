// cmd/gateway/main.go
//
// CHANGES IN THIS REVISION (Phase 1.5):
//   - WakeDisplayService constructed and injected into api.NewServer.
//     All other wiring is unchanged.
package main

import (
	"context"
	"log/slog"
	"os"
	"os/signal"
	"syscall"
	"time"

	"unbot-gateway/internal/api"
	"unbot-gateway/internal/catalog"
	"unbot-gateway/internal/config"
	"unbot-gateway/internal/database"
	mqttclient "unbot-gateway/internal/mqtt"
	"unbot-gateway/internal/order_items"
	"unbot-gateway/internal/orders"
	"unbot-gateway/internal/services"
)

func main() {
	log := slog.New(slog.NewJSONHandler(os.Stdout, &slog.HandlerOptions{
		Level: slog.LevelInfo,
	}))

	// ── Config ────────────────────────────────────────────────────────────
	cfg, err := config.Load()
	if err != nil {
		log.Error("configuration error", "error", err)
		os.Exit(1)
	}
	log.Info("configuration loaded",
		"mqtt_host", cfg.MQTTHost,
		"mqtt_port", cfg.MQTTPort,
		"http_addr", cfg.HTTPAddr,
		"database_url", cfg.DatabaseURL,
	)

	// ── MQTT client ───────────────────────────────────────────────────────
	mqtt := mqttclient.NewClient(cfg, log)
	if err := mqtt.Connect(); err != nil {
		log.Warn("MQTT connect failed (continuing without MQTT)", "error", err)
		// Continue without MQTT - HTTP API will still work
	}

	// ── Service layer ─────────────────────────────────────────────────────
	otpSvc := services.NewOTPService(mqtt)
	orderSvc := services.NewOrderService(otpSvc, mqtt, log)
	wakeSvc := services.NewWakeDisplayService(otpSvc, mqtt, log) // NEW

	// ── Database / catalog ────────────────────────────────────────────────
	dbCtx, dbCancel := context.WithTimeout(context.Background(), 5*time.Second)
	dbPool, err := database.NewPostgresPool(dbCtx, cfg.DatabaseURL)
	dbCancel()
	if err != nil {
		log.Error("database connection failed", "error", err)
		os.Exit(1)
	}
	defer dbPool.Close()

	catalogRepo := catalog.NewRepository(dbPool)
	catalogSvc := catalog.NewService(catalogRepo)

	orderItemsRepo := order_items.NewRepository(dbPool)
	orderItemsSvc := order_items.NewService(orderItemsRepo, log)

	ordersRepo := orders.NewRepository(dbPool, orderItemsRepo)
	ordersSvc := orders.NewService(ordersRepo, orderItemsSvc, log)

	// ── HTTP server ───────────────────────────────────────────────────────
	srv := api.NewServer(cfg.HTTPAddr, log, otpSvc, orderSvc, wakeSvc, catalogSvc, ordersSvc)
	srv.Start()

	log.Info("gateway ready",
		"endpoints", []string{
			"GET   /health",
			"GET   /api/restaurants",
			"GET   /api/restaurants/{id}",
			"GET   /api/restaurants/{id}/products",
			"POST  /api/orders",
			"GET   /api/orders/{id}",
			"GET   /api/clients/{client_id}/orders",
			"PATCH /api/orders/{id}/status",
			"POST  /api/validate-code",
			"POST  /api/orders/{id}/dispatch",
			"POST  /api/orders/{id}/wake-display",
		},
	)

	// ── Block on signal ───────────────────────────────────────────────────
	quit := make(chan os.Signal, 1)
	signal.Notify(quit, syscall.SIGINT, syscall.SIGTERM)
	<-quit

	log.Info("shutdown signal received — draining...")

	shutdownCtx, cancel := context.WithTimeout(context.Background(), 10*time.Second)
	defer cancel()

	if err := srv.Shutdown(shutdownCtx); err != nil {
		log.Error("HTTP shutdown error", "error", err)
	}

	mqtt.Disconnect()
	log.Info("gateway stopped cleanly")
}
