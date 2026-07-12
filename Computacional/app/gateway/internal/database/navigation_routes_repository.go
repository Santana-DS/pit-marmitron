package database

import (
	"context"
	"fmt"

	"github.com/jackc/pgx/v5/pgxpool"
	"unbot-gateway/internal/services"
)

type NavigationRouteRepository struct{ pool *pgxpool.Pool }

func NewNavigationRouteRepository(pool *pgxpool.Pool) *NavigationRouteRepository {
	return &NavigationRouteRepository{pool: pool}
}

func (r *NavigationRouteRepository) ResolveRoute(ctx context.Context, pointKey string) (services.NavigationRoute, error) {
	const routeQuery = `SELECT route_id FROM navigation_routes
		WHERE destination_point_key=$1 AND is_active=true AND execution_ready=true
		ORDER BY version DESC LIMIT 1`
	var route services.NavigationRoute
	if err := r.pool.QueryRow(ctx, routeQuery, pointKey).Scan(&route.RouteID); err != nil {
		return services.NavigationRoute{}, fmt.Errorf("resolve route for %s: %w", pointKey, err)
	}
	rows, err := r.pool.Query(ctx, `SELECT sequence, latitude, longitude FROM navigation_route_nodes WHERE route_id=$1 ORDER BY sequence`, route.RouteID)
	if err != nil {
		return services.NavigationRoute{}, fmt.Errorf("load route nodes: %w", err)
	}
	defer rows.Close()
	for rows.Next() {
		var node services.RouteNode
		if err := rows.Scan(&node.Sequence, &node.Latitude, &node.Longitude); err != nil {
			return services.NavigationRoute{}, err
		}
		route.Nodes = append(route.Nodes, node)
	}
	if err := rows.Err(); err != nil {
		return services.NavigationRoute{}, err
	}
	if len(route.Nodes) == 0 {
		return services.NavigationRoute{}, fmt.Errorf("route %s has no nodes", route.RouteID)
	}
	return route, nil
}
