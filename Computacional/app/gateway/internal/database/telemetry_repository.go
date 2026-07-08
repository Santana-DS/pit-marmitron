package database

import (
	"context"
	"fmt"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"unbot-gateway/internal/services"
)

type TelemetryRepository struct {
	pool *pgxpool.Pool
}

func NewTelemetryRepository(pool *pgxpool.Pool) *TelemetryRepository {
	return &TelemetryRepository{pool: pool}
}

var telemetryColumns = []string{
	"order_id", "ts", "nav_state",
	"pose_x", "pose_y", "pose_theta", "map_frame",
	"linear_speed_mps", "avg_speed_mps",
	"battery_percent", "battery_voltage_v",
	"remaining_m", "progress_pct", "eta_seconds",
	"cpu_pct", "mem_pct",
}

func (r *TelemetryRepository) InsertBatch(ctx context.Context, points []services.TelemetryPoint) error {
	if len(points) == 0 {
		return nil
	}

	rows := make([][]any, 0, len(points))
	for _, p := range points {
		rows = append(rows, []any{
			p.OrderID, p.Ts, p.NavState,
			p.PoseX, p.PoseY, p.PoseTheta, p.MapFrame,
			p.LinearSpeedMps, p.AvgSpeedMps,
			p.BatteryPercent, p.BatteryVoltageV,
			p.RemainingM, p.ProgressPct, p.EtaSeconds,
			p.CpuPct, p.MemPct,
		})
	}

	_, err := r.pool.CopyFrom(
		ctx,
		pgx.Identifier{"robot_telemetry"},
		telemetryColumns,
		pgx.CopyFromRows(rows),
	)
	if err != nil {
		return fmt.Errorf("telemetry batch copy failed: %w", err)
	}
	return nil
}
