package database

import (
	"context"
	"fmt"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"unbot-gateway/internal/services"
)

type TelemetryRepository struct {
	pool *pgxpool.Pool
}

// TelemetryHistoryPoint is the durable, nullable representation used for
// operator replay. Pose is absent until the ROS integration publishes it.
type TelemetryHistoryPoint struct {
	Timestamp      time.Time
	PoseX          *float64
	PoseY          *float64
	PoseTheta      *float64
	MapFrame       *string
	BatteryPercent *float64
	NavState       string
	RemainingM     *float64
	ProgressPct    *float64
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

// ListByOrder returns an ordered, bounded telemetry trail for operator replay.
func (r *TelemetryRepository) ListByOrder(
	ctx context.Context,
	orderID string,
	from *time.Time,
) ([]TelemetryHistoryPoint, error) {
	const query = `
		SELECT ts, pose_x, pose_y, pose_theta, map_frame,
		       battery_percent, nav_state, remaining_m, progress_pct
		FROM robot_telemetry
		WHERE order_id = $1 AND ($2::timestamptz IS NULL OR ts >= $2)
		ORDER BY ts ASC
		LIMIT 5000
	`

	rows, err := r.pool.Query(ctx, query, orderID, from)
	if err != nil {
		return nil, fmt.Errorf("list telemetry by order: %w", err)
	}
	defer rows.Close()

	points := make([]TelemetryHistoryPoint, 0)
	for rows.Next() {
		var point TelemetryHistoryPoint
		if err := rows.Scan(
			&point.Timestamp,
			&point.PoseX,
			&point.PoseY,
			&point.PoseTheta,
			&point.MapFrame,
			&point.BatteryPercent,
			&point.NavState,
			&point.RemainingM,
			&point.ProgressPct,
		); err != nil {
			return nil, fmt.Errorf("scan telemetry point: %w", err)
		}
		points = append(points, point)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate telemetry points: %w", err)
	}
	return points, nil
}
