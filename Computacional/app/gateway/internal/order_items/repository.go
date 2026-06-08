package order_items

import (
	"context"
	"errors"
	"fmt"

	"github.com/jackc/pgx/v5"
	"github.com/jackc/pgx/v5/pgxpool"
)

var (
	ErrItemNotFound = errors.New("order item not found")
)

// Repository handles database operations for order items
type Repository struct {
	db *pgxpool.Pool
}

func NewRepository(db *pgxpool.Pool) *Repository {
	return &Repository{db: db}
}

// CreateItems creates multiple order items in a transaction
// This method expects to be called within an existing transaction
func (r *Repository) CreateItems(ctx context.Context, tx pgx.Tx, orderID string, items []CreateItemRequest) ([]OrderItem, error) {
	const insertItemSQL = `
		INSERT INTO order_items (
			order_id,
			product_id,
			product_name,
			product_description,
			product_emoji,
			quantity,
			unit_price_cents,
			total_price_cents
		)
		SELECT 
			$1,
			$2,
			p.name,
			p.description,
			p.emoji,
			$3,
			$4,
			$5
		FROM products p
		WHERE p.id = $2
		RETURNING 
			id::text,
			order_id::text,
			product_id::text,
			product_name,
			product_description,
			product_emoji,
			quantity,
			unit_price_cents,
			total_price_cents,
			created_at
	`

	createdItems := make([]OrderItem, 0, len(items))

	for _, item := range items {
		totalPriceCents := item.UnitPriceCents * item.Quantity

		var orderItem OrderItem
		err := tx.QueryRow(ctx, insertItemSQL,
			orderID,
			item.ProductID,
			item.Quantity,
			item.UnitPriceCents,
			totalPriceCents,
		).Scan(
			&orderItem.ID,
			&orderItem.OrderID,
			&orderItem.ProductID,
			&orderItem.ProductName,
			&orderItem.ProductDescription,
			&orderItem.ProductEmoji,
			&orderItem.Quantity,
			&orderItem.UnitPriceCents,
			&orderItem.TotalPriceCents,
			&orderItem.CreatedAt,
		)
		if err != nil {
			return nil, fmt.Errorf("insert order item: %w", err)
		}
		createdItems = append(createdItems, orderItem)
	}

	return createdItems, nil
}

// GetItemsByOrderID retrieves all items for a specific order
func (r *Repository) GetItemsByOrderID(ctx context.Context, orderID string) ([]OrderItem, error) {
	const itemsSQL = `
		SELECT
			id::text,
			order_id::text,
			product_id::text,
			product_name,
			product_description,
			product_emoji,
			quantity,
			unit_price_cents,
			total_price_cents,
			created_at
		FROM order_items
		WHERE order_id = $1
		ORDER BY created_at ASC
	`

	rows, err := r.db.Query(ctx, itemsSQL, orderID)
	if err != nil {
		return nil, fmt.Errorf("query order items: %w", err)
	}
	defer rows.Close()

	items := make([]OrderItem, 0)
	for rows.Next() {
		var item OrderItem
		if err := rows.Scan(
			&item.ID,
			&item.OrderID,
			&item.ProductID,
			&item.ProductName,
			&item.ProductDescription,
			&item.ProductEmoji,
			&item.Quantity,
			&item.UnitPriceCents,
			&item.TotalPriceCents,
			&item.CreatedAt,
		); err != nil {
			return nil, fmt.Errorf("scan order item: %w", err)
		}
		items = append(items, item)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate order items: %w", err)
	}

	return items, nil
}

// GetItemsByOrderIDs retrieves all items for multiple orders in a single query
// This prevents N+1 query problems when loading multiple orders with their items
func (r *Repository) GetItemsByOrderIDs(ctx context.Context, orderIDs []string) (map[string][]OrderItem, error) {
	if len(orderIDs) == 0 {
		return make(map[string][]OrderItem), nil
	}

	const itemsSQL = `
		SELECT
			id::text,
			order_id::text,
			product_id::text,
			product_name,
			product_description,
			product_emoji,
			quantity,
			unit_price_cents,
			total_price_cents,
			created_at
		FROM order_items
		WHERE order_id = ANY($1)
		ORDER BY order_id, created_at ASC
	`

	rows, err := r.db.Query(ctx, itemsSQL, orderIDs)
	if err != nil {
		return nil, fmt.Errorf("query order items by order ids: %w", err)
	}
	defer rows.Close()

	// Group items by order_id
	itemsByOrder := make(map[string][]OrderItem)
	for rows.Next() {
		var item OrderItem
		if err := rows.Scan(
			&item.ID,
			&item.OrderID,
			&item.ProductID,
			&item.ProductName,
			&item.ProductDescription,
			&item.ProductEmoji,
			&item.Quantity,
			&item.UnitPriceCents,
			&item.TotalPriceCents,
			&item.CreatedAt,
		); err != nil {
			return nil, fmt.Errorf("scan order item: %w", err)
		}
		itemsByOrder[item.OrderID] = append(itemsByOrder[item.OrderID], item)
	}

	if err := rows.Err(); err != nil {
		return nil, fmt.Errorf("iterate order items: %w", err)
	}

	return itemsByOrder, nil
}

// CalculateSubtotal calculates the subtotal from a list of items
func (r *Repository) CalculateSubtotal(items []CreateItemRequest) int {
	subtotal := 0
	for _, item := range items {
		subtotal += item.UnitPriceCents * item.Quantity
	}
	return subtotal
}
