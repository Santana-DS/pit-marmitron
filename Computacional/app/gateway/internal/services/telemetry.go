package services

import (
	"context"
	"encoding/json"
	"log/slog"
	"sync/atomic"
	"time"
)

type telemetryPosePayload struct {
	X     float64 `json:"x"`
	Y     float64 `json:"y"`
	Theta float64 `json:"theta"`
	Frame string  `json:"frame"`
}

type telemetryVelocityPayload struct {
	LinearMps float64 `json:"linear_mps"`
}

type telemetryBatteryPayload struct {
	Percent  float64 `json:"percent"`
	VoltageV float64 `json:"voltage_v"`
}

type telemetryWirePayload struct {
	Source        string                   `json:"source"`
	Timestamp     int64                    `json:"timestamp"`
	Pose          *telemetryPosePayload    `json:"pose"`
	Velocity      telemetryVelocityPayload `json:"velocity"`
	Battery       telemetryBatteryPayload  `json:"battery"`
	NavState      string                   `json:"nav_state"`
	ActiveOrderID string                   `json:"active_order_id"`
	RemainingM    float64                  `json:"remaining_m"`
	ProgressPct   float64                  `json:"progress_pct"`
	AvgSpeedMps   float64                  `json:"avg_speed_mps"`
	EtaSeconds    float64                  `json:"eta_seconds"`
	CpuPct        float64                  `json:"cpu_pct"`
	MemPct        float64                  `json:"mem_pct"`
}

type TelemetryPoint struct {
	OrderID         string
	Ts              time.Time
	NavState        string
	PoseX           float64
	PoseY           float64
	PoseTheta       float64
	MapFrame        string
	LinearSpeedMps  float64
	AvgSpeedMps     float64
	BatteryPercent  float64
	BatteryVoltageV float64
	RemainingM      float64
	ProgressPct     float64
	EtaSeconds      float64
	CpuPct          float64
	MemPct          float64
}

type TelemetryRepository interface {
	InsertBatch(ctx context.Context, points []TelemetryPoint) error
}

const (
	telemetryChanCapacity  = 500
	telemetryBatchSize     = 50
	telemetryFlushInterval = 2 * time.Second
)

type TelemetryIngestService struct {
	repo TelemetryRepository
	log  *slog.Logger

	telemetryChan chan TelemetryPoint

	droppedCounter atomic.Int64
	skippedCounter atomic.Int64
}

func NewTelemetryIngestService(repo TelemetryRepository, log *slog.Logger) *TelemetryIngestService {
	return &TelemetryIngestService{
		repo:          repo,
		log:           log,
		telemetryChan: make(chan TelemetryPoint, telemetryChanCapacity),
	}
}

func (s *TelemetryIngestService) EnqueueTelemetry(payload []byte) {
	var wp telemetryWirePayload
	if err := json.Unmarshal(payload, &wp); err != nil {
		s.log.Warn("telemetry: malformed payload", "error", err)
		return
	}

	if wp.ActiveOrderID == "" {
		s.skippedCounter.Add(1)
		return
	}

	pt := TelemetryPoint{
		OrderID:         wp.ActiveOrderID,
		Ts:              telemetryTimestamp(wp.Timestamp),
		NavState:        wp.NavState,
		LinearSpeedMps:  wp.Velocity.LinearMps,
		AvgSpeedMps:     wp.AvgSpeedMps,
		BatteryPercent:  wp.Battery.Percent,
		BatteryVoltageV: wp.Battery.VoltageV,
		RemainingM:      wp.RemainingM,
		ProgressPct:     wp.ProgressPct,
		EtaSeconds:      wp.EtaSeconds,
		CpuPct:          wp.CpuPct,
		MemPct:          wp.MemPct,
	}
	if wp.Pose != nil {
		pt.PoseX = wp.Pose.X
		pt.PoseY = wp.Pose.Y
		pt.PoseTheta = wp.Pose.Theta
		pt.MapFrame = wp.Pose.Frame
	}

	select {
	case s.telemetryChan <- pt:
	default:
		s.droppedCounter.Add(1)
	}
}

func (s *TelemetryIngestService) RunBatcher(ctx context.Context) {
	batch := make([]TelemetryPoint, 0, telemetryBatchSize)
	ticker := time.NewTicker(telemetryFlushInterval)
	defer ticker.Stop()

	flush := func() {
		if len(batch) == 0 {
			return
		}
		if err := s.repo.InsertBatch(ctx, batch); err != nil {
			s.log.Error("telemetry batch insert failed", "error", err, "size", len(batch))
		}
		batch = batch[:0]
	}

	for {
		select {
		case pt := <-s.telemetryChan:
			batch = append(batch, pt)
			if len(batch) >= telemetryBatchSize {
				flush()
			}
		case <-ticker.C:
			flush()
		case <-ctx.Done():
			for {
				select {
				case pt := <-s.telemetryChan:
					batch = append(batch, pt)
					if len(batch) >= telemetryBatchSize {
						flush()
					}
				default:
					flush()
					s.log.Info("telemetry batcher stopped",
						"dropped", s.droppedCounter.Load(),
						"skipped_idle", s.skippedCounter.Load(),
					)
					return
				}
			}
		}
	}
}

func telemetryTimestamp(unixSeconds int64) time.Time {
	if unixSeconds <= 0 {
		return time.Now().UTC()
	}
	return time.Unix(unixSeconds, 0).UTC()
}
