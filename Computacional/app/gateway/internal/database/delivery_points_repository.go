package database

import (
	"context"
	"errors"
	"fmt"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"unbot-gateway/internal/services"
)

var ErrDeliveryPointNotFound = errors.New("delivery point not found")

type DeliveryPoint struct {
	PointKey       string  `json:"point_key"`
	Label          string  `json:"label"`
	DisplayAddress string  `json:"display_address"`
	Latitude       float64 `json:"latitude"`
	Longitude      float64 `json:"longitude"`
	MapX           float64 `json:"map_x"`
	MapY           float64 `json:"map_y"`
	MapTheta       float64 `json:"map_theta"`
	MapFrame       string  `json:"map_frame"`
}

type DeliveryPointRepository struct {
	pool *pgxpool.Pool
}

func NewDeliveryPointRepository(pool *pgxpool.Pool) *DeliveryPointRepository {
	return &DeliveryPointRepository{pool: pool}
}

func (r *DeliveryPointRepository) ListActive(ctx context.Context) ([]DeliveryPoint, error) {
	const query = `
		SELECT point_key, label, display_address, latitude, longitude,
		       map_x, map_y, map_theta, map_frame
		FROM delivery_points
		WHERE is_active = true
		ORDER BY sort_order ASC, label ASC
	`
	rows, err := r.pool.Query(ctx, query)
	if err != nil {
		return nil, fmt.Errorf("list active delivery points: %w", err)
	}
	defer rows.Close()

	points := make([]DeliveryPoint, 0)
	for rows.Next() {
		var point DeliveryPoint
		if err := rows.Scan(
			&point.PointKey, &point.Label, &point.DisplayAddress,
			&point.Latitude, &point.Longitude, &point.MapX, &point.MapY,
			&point.MapTheta, &point.MapFrame,
		); err != nil {
			return nil, fmt.Errorf("scan delivery point: %w", err)
		}
		points = append(points, point)
	}
	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate delivery points: %w", err)
	}
	return points, nil
}

func (r *DeliveryPointRepository) ResolveWaypoint(
	ctx context.Context,
	pointKey string,
) (services.Waypoint, error) {
	const query = `
		SELECT label, map_x, map_y, map_theta, map_frame
		FROM delivery_points
		WHERE point_key = $1 AND is_active = true
	`
	var point services.Waypoint
	point.Name = pointKey
	if err := r.pool.QueryRow(ctx, query, pointKey).Scan(
		&point.Name, &point.X, &point.Y, &point.Theta, &point.MapFrame,
	); err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return services.Waypoint{}, ErrDeliveryPointNotFound
		}
		return services.Waypoint{}, fmt.Errorf("resolve delivery point: %w", err)
	}
	return point, nil
}
