package catalog

import (
	"context"
	"errors"
	"fmt"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

var ErrRestaurantNotFound = errors.New("restaurant not found")

type Repository struct {
	db *pgxpool.Pool
}

func NewRepository(db *pgxpool.Pool) *Repository {
	return &Repository{db: db}
}

func (r *Repository) ListRestaurants(ctx context.Context, query string) ([]Restaurant, error) {
	const sql = `
		SELECT
			id::text,
			name,
			emoji,
			bg_color,
			rating::float8,
			eta_minutes,
			is_open
		FROM restaurants
		WHERE deleted_at IS NULL
		  AND (
				$1 = ''
				OR name ILIKE '%' || $1 || '%'
				OR EXISTS (
					SELECT 1
					FROM products p
					WHERE p.restaurant_id = restaurants.id
					  AND p.deleted_at IS NULL
					  AND p.is_available = true
					  AND p.name ILIKE '%' || $1 || '%'
				)
		  )
		ORDER BY is_open DESC, rating DESC, name ASC
	`

	rows, err := r.db.Query(ctx, sql, query)
	if err != nil {
		return nil, fmt.Errorf("query restaurants: %w", err)
	}
	defer rows.Close()

	restaurants := make([]Restaurant, 0)
	for rows.Next() {
		var item Restaurant
		if err := rows.Scan(
			&item.ID,
			&item.Name,
			&item.Emoji,
			&item.BgColor,
			&item.Rating,
			&item.ETAMinutes,
			&item.IsOpen,
		); err != nil {
			return nil, fmt.Errorf("scan restaurant: %w", err)
		}
		restaurants = append(restaurants, item)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate restaurants: %w", err)
	}

	return restaurants, nil
}

func (r *Repository) GetRestaurantByID(ctx context.Context, restaurantID string) (*Restaurant, error) {
	const sql = `
		SELECT
			id::text,
			name,
			emoji,
			bg_color,
			rating::float8,
			eta_minutes,
			is_open
		FROM restaurants
		WHERE id = $1
		  AND deleted_at IS NULL
	`

	var item Restaurant
	err := r.db.QueryRow(ctx, sql, restaurantID).Scan(
		&item.ID,
		&item.Name,
		&item.Emoji,
		&item.BgColor,
		&item.Rating,
		&item.ETAMinutes,
		&item.IsOpen,
	)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, ErrRestaurantNotFound
		}
		return nil, fmt.Errorf("get restaurant by id: %w", err)
	}

	return &item, nil
}

func (r *Repository) ListProductsByRestaurant(ctx context.Context, restaurantID string) ([]Product, error) {
	const sql = `
		SELECT
			id::text,
			restaurant_id::text,
			name,
			description,
			emoji,
			price_cents,
			is_available,
			sort_order
		FROM products
		WHERE restaurant_id = $1
		  AND deleted_at IS NULL
		ORDER BY sort_order ASC, name ASC
	`

	rows, err := r.db.Query(ctx, sql, restaurantID)
	if err != nil {
		return nil, fmt.Errorf("query products by restaurant: %w", err)
	}
	defer rows.Close()

	products := make([]Product, 0)
	for rows.Next() {
		var item Product
		if err := rows.Scan(
			&item.ID,
			&item.RestaurantID,
			&item.Name,
			&item.Description,
			&item.Emoji,
			&item.PriceCents,
			&item.IsAvailable,
			&item.SortOrder,
		); err != nil {
			return nil, fmt.Errorf("scan product: %w", err)
		}
		products = append(products, item)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate products: %w", err)
	}

	return products, nil
}
