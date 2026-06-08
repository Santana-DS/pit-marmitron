package orders

import (
	"context"
	"errors"
	"fmt"
	"math/rand"
	"time"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"

	"unbot-gateway/internal/order_items"
)

var (
	ErrOrderNotFound = errors.New("order not found")
	ErrInvalidStatus = errors.New("invalid order status")
)

type Repository struct {
	db        *pgxpool.Pool
	itemsRepo *order_items.Repository
}

func NewRepository(db *pgxpool.Pool, itemsRepo *order_items.Repository) *Repository {
	return &Repository{
		db:        db,
		itemsRepo: itemsRepo,
	}
}

// CreateOrder creates a new order with its items in a transaction
func (r *Repository) CreateOrder(ctx context.Context, req CreateOrderRequest) (*OrderWithItems, error) {
	tx, err := r.db.Begin(ctx)
	if err != nil {
		return nil, fmt.Errorf("begin transaction: %w", err)
	}
	defer tx.Rollback(ctx)

	// Generate a unique public code (6-digit)
	publicCode := generatePublicCode()

	// Convert request items to order_items format
	itemRequests := make([]order_items.CreateItemRequest, len(req.Items))
	for i, item := range req.Items {
		itemRequests[i] = order_items.CreateItemRequest{
			ProductID:      item.ProductID,
			Quantity:       item.Quantity,
			UnitPriceCents: item.UnitPriceCents,
		}
	}

	// Calculate subtotal using items repository
	subtotalCents := r.itemsRepo.CalculateSubtotal(itemRequests)

	// Calculate total
	totalCents := subtotalCents + req.DeliveryFeeCents - req.DiscountCents

	// Insert order
	const insertOrderSQL = `
		INSERT INTO orders (
			public_code,
			client_user_id,
			restaurant_id,
			delivery_address,
			status,
			subtotal_cents,
			delivery_fee_cents,
			discount_cents,
			total_cents,
			notes,
			placed_at
		) VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11)
		RETURNING 
			id::text,
			public_code,
			client_user_id::text,
			restaurant_id::text,
			delivery_address,
			status,
			subtotal_cents,
			delivery_fee_cents,
			discount_cents,
			total_cents,
			robot_dispatched,
			gateway_mode,
			mqtt_connected,
			placed_at,
			dispatched_at,
			completed_at,
			cancelled_at,
			cancel_reason,
			notes,
			created_at,
			updated_at
	`

	var order Order
	err = tx.QueryRow(ctx, insertOrderSQL,
		publicCode,
		req.ClientUserID,
		req.RestaurantID,
		req.DeliveryAddress,
		StatusPending,
		subtotalCents,
		req.DeliveryFeeCents,
		req.DiscountCents,
		totalCents,
		req.Notes,
		time.Now(),
	).Scan(
		&order.ID,
		&order.PublicCode,
		&order.ClientUserID,
		&order.RestaurantID,
		&order.DeliveryAddress,
		&order.Status,
		&order.SubtotalCents,
		&order.DeliveryFeeCents,
		&order.DiscountCents,
		&order.TotalCents,
		&order.RobotDispatched,
		&order.GatewayMode,
		&order.MQTTConnected,
		&order.PlacedAt,
		&order.DispatchedAt,
		&order.CompletedAt,
		&order.CancelledAt,
		&order.CancelReason,
		&order.Notes,
		&order.CreatedAt,
		&order.UpdatedAt,
	)
	if err != nil {
		return nil, fmt.Errorf("insert order: %w", err)
	}

	// Create order items using items repository
	items, err := r.itemsRepo.CreateItems(ctx, tx, order.ID, itemRequests)
	if err != nil {
		return nil, fmt.Errorf("create order items: %w", err)
	}

	if err := tx.Commit(ctx); err != nil {
		return nil, fmt.Errorf("commit transaction: %w", err)
	}

	return &OrderWithItems{
		Order: order,
		Items: items,
	}, nil
}

// GetOrderByID retrieves an order with its items by ID
func (r *Repository) GetOrderByID(ctx context.Context, orderID string) (*OrderWithItems, error) {
	const orderSQL = `
		SELECT
			id::text,
			public_code,
			client_user_id::text,
			restaurant_id::text,
			delivery_address,
			status,
			subtotal_cents,
			delivery_fee_cents,
			discount_cents,
			total_cents,
			robot_dispatched,
			gateway_mode,
			mqtt_connected,
			placed_at,
			dispatched_at,
			completed_at,
			cancelled_at,
			cancel_reason,
			notes,
			created_at,
			updated_at
		FROM orders
		WHERE id = $1
	`

	var order Order
	err := r.db.QueryRow(ctx, orderSQL, orderID).Scan(
		&order.ID,
		&order.PublicCode,
		&order.ClientUserID,
		&order.RestaurantID,
		&order.DeliveryAddress,
		&order.Status,
		&order.SubtotalCents,
		&order.DeliveryFeeCents,
		&order.DiscountCents,
		&order.TotalCents,
		&order.RobotDispatched,
		&order.GatewayMode,
		&order.MQTTConnected,
		&order.PlacedAt,
		&order.DispatchedAt,
		&order.CompletedAt,
		&order.CancelledAt,
		&order.CancelReason,
		&order.Notes,
		&order.CreatedAt,
		&order.UpdatedAt,
	)
	if err != nil {
		if errors.Is(err, pgx.ErrNoRows) {
			return nil, ErrOrderNotFound
		}
		return nil, fmt.Errorf("get order: %w", err)
	}

	// Get order items using items repository
	items, err := r.itemsRepo.GetItemsByOrderID(ctx, orderID)
	if err != nil {
		return nil, fmt.Errorf("get order items: %w", err)
	}

	return &OrderWithItems{
		Order: order,
		Items: items,
	}, nil
}

// ListOrdersByClient retrieves orders for a specific client
// Optimized to prevent N+1 query problem by batch loading all items
func (r *Repository) ListOrdersByClient(ctx context.Context, clientUserID string, limit, offset int) ([]OrderWithItems, error) {
	const ordersSQL = `
		SELECT
			id::text,
			public_code,
			client_user_id::text,
			restaurant_id::text,
			delivery_address,
			status,
			subtotal_cents,
			delivery_fee_cents,
			discount_cents,
			total_cents,
			robot_dispatched,
			gateway_mode,
			mqtt_connected,
			placed_at,
			dispatched_at,
			completed_at,
			cancelled_at,
			cancel_reason,
			notes,
			created_at,
			updated_at
		FROM orders
		WHERE client_user_id = $1
		ORDER BY placed_at DESC
		LIMIT $2 OFFSET $3
	`

	rows, err := r.db.Query(ctx, ordersSQL, clientUserID, limit, offset)
	if err != nil {
		return nil, fmt.Errorf("query orders: %w", err)
	}
	defer rows.Close()

	// First, collect all orders and their IDs
	ordersList := make([]Order, 0)
	orderIDs := make([]string, 0)

	for rows.Next() {
		var order Order
		if err := rows.Scan(
			&order.ID,
			&order.PublicCode,
			&order.ClientUserID,
			&order.RestaurantID,
			&order.DeliveryAddress,
			&order.Status,
			&order.SubtotalCents,
			&order.DeliveryFeeCents,
			&order.DiscountCents,
			&order.TotalCents,
			&order.RobotDispatched,
			&order.GatewayMode,
			&order.MQTTConnected,
			&order.PlacedAt,
			&order.DispatchedAt,
			&order.CompletedAt,
			&order.CancelledAt,
			&order.CancelReason,
			&order.Notes,
			&order.CreatedAt,
			&order.UpdatedAt,
		); err != nil {
			return nil, fmt.Errorf("scan order: %w", err)
		}
		ordersList = append(ordersList, order)
		orderIDs = append(orderIDs, order.ID)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate orders: %w", err)
	}

	// Batch load all items for all orders in a single query
	itemsByOrderID, err := r.itemsRepo.GetItemsByOrderIDs(ctx, orderIDs)
	if err != nil {
		return nil, fmt.Errorf("batch get order items: %w", err)
	}

	// Combine orders with their items
	orders := make([]OrderWithItems, 0, len(ordersList))
	for _, order := range ordersList {
		items := itemsByOrderID[order.ID]
		if items == nil {
			items = make([]order_items.OrderItem, 0)
		}
		orders = append(orders, OrderWithItems{
			Order: order,
			Items: items,
		})
	}

	return orders, nil
}

// ListOrdersByRestaurant retrieves orders for a specific restaurant
// Optimized to prevent N+1 query problem by batch loading all items
func (r *Repository) ListOrdersByRestaurant(ctx context.Context, restaurantID string, limit, offset int) ([]OrderWithItems, error) {
	const ordersSQL = `
		SELECT
			id::text,
			public_code,
			client_user_id::text,
			restaurant_id::text,
			delivery_address,
			status,
			subtotal_cents,
			delivery_fee_cents,
			discount_cents,
			total_cents,
			robot_dispatched,
			gateway_mode,
			mqtt_connected,
			placed_at,
			dispatched_at,
			completed_at,
			cancelled_at,
			cancel_reason,
			notes,
			created_at,
			updated_at
		FROM orders
		WHERE restaurant_id = $1
		ORDER BY placed_at DESC
		LIMIT $2 OFFSET $3
	`

	rows, err := r.db.Query(ctx, ordersSQL, restaurantID, limit, offset)
	if err != nil {
		return nil, fmt.Errorf("query restaurant orders: %w", err)
	}
	defer rows.Close()

	ordersList := make([]Order, 0)
	orderIDs := make([]string, 0)

	for rows.Next() {
		var order Order
		if err := rows.Scan(
			&order.ID,
			&order.PublicCode,
			&order.ClientUserID,
			&order.RestaurantID,
			&order.DeliveryAddress,
			&order.Status,
			&order.SubtotalCents,
			&order.DeliveryFeeCents,
			&order.DiscountCents,
			&order.TotalCents,
			&order.RobotDispatched,
			&order.GatewayMode,
			&order.MQTTConnected,
			&order.PlacedAt,
			&order.DispatchedAt,
			&order.CompletedAt,
			&order.CancelledAt,
			&order.CancelReason,
			&order.Notes,
			&order.CreatedAt,
			&order.UpdatedAt,
		); err != nil {
			return nil, fmt.Errorf("scan restaurant order: %w", err)
		}
		ordersList = append(ordersList, order)
		orderIDs = append(orderIDs, order.ID)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate restaurant orders: %w", err)
	}

	itemsByOrderID, err := r.itemsRepo.GetItemsByOrderIDs(ctx, orderIDs)
	if err != nil {
		return nil, fmt.Errorf("batch get restaurant order items: %w", err)
	}

	orders := make([]OrderWithItems, 0, len(ordersList))
	for _, order := range ordersList {
		items := itemsByOrderID[order.ID]
		if items == nil {
			items = make([]order_items.OrderItem, 0)
		}
		orders = append(orders, OrderWithItems{
			Order: order,
			Items: items,
		})
	}

	return orders, nil
}

// UpdateOrderStatus updates the status of an order
func (r *Repository) UpdateOrderStatus(ctx context.Context, orderID, status string, cancelReason *string) error {
	// Validate status
	validStatuses := map[string]bool{
		StatusPending:   true,
		StatusPreparing: true,
		StatusOnTheWay:  true,
		StatusDelivered: true,
		StatusCancelled: true,
	}
	if !validStatuses[status] {
		return ErrInvalidStatus
	}

	now := time.Now()
	var sql string
	var args []interface{}

	switch status {
	case StatusCancelled:
		sql = `
			UPDATE orders
			SET status = $1, cancelled_at = $2, cancel_reason = $3, updated_at = $4
			WHERE id = $5
		`
		args = []interface{}{status, now, cancelReason, now, orderID}
	case StatusDelivered:
		sql = `
			UPDATE orders
			SET status = $1, completed_at = $2, updated_at = $3
			WHERE id = $4
		`
		args = []interface{}{status, now, now, orderID}
	case StatusOnTheWay:
		sql = `
			UPDATE orders
			SET status = $1, dispatched_at = $2, updated_at = $3
			WHERE id = $4
		`
		args = []interface{}{status, now, now, orderID}
	default:
		sql = `
			UPDATE orders
			SET status = $1, updated_at = $2
			WHERE id = $3
		`
		args = []interface{}{status, now, orderID}
	}

	result, err := r.db.Exec(ctx, sql, args...)
	if err != nil {
		return fmt.Errorf("update order status: %w", err)
	}

	if result.RowsAffected() == 0 {
		return ErrOrderNotFound
	}

	return nil
}

// generatePublicCode generates a random 6-digit code for the order
func generatePublicCode() string {
	return fmt.Sprintf("%06d", rand.Intn(1000000))
}
