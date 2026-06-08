package order_items

import (
	"context"
	"fmt"
	"log/slog"

	"github.com/jackc/pgx/v5"
)

// RepositoryInterface defines the contract for order items repository operations
type RepositoryInterface interface {
	CreateItems(ctx context.Context, tx pgx.Tx, orderID string, items []CreateItemRequest) ([]OrderItem, error)
	GetItemsByOrderID(ctx context.Context, orderID string) ([]OrderItem, error)
	CalculateSubtotal(items []CreateItemRequest) int
}

// ServiceInterface defines the contract for order items service operations
type ServiceInterface interface {
	ValidateItems(items []CreateItemRequest) error
	CalculateSubtotal(items []CreateItemRequest) int
	ValidateItemTotals(items []OrderItem) error
	GetItemsByOrderID(ctx context.Context, orderID string) ([]OrderItem, error)
	CreateItems(ctx context.Context, tx pgx.Tx, orderID string, items []CreateItemRequest) ([]OrderItem, error)
}

// Service handles business logic for order items
type Service struct {
	repo RepositoryInterface
	log  *slog.Logger
}

func NewService(repo RepositoryInterface, log *slog.Logger) *Service {
	return &Service{
		repo: repo,
		log:  log,
	}
}

// ValidateItems validates a list of order items
func (s *Service) ValidateItems(items []CreateItemRequest) error {
	if len(items) == 0 {
		return &ItemValidationError{
			Field:   "items",
			Message: "at least one item is required",
		}
	}

	for i, item := range items {
		if err := s.validateItem(i, item); err != nil {
			return err
		}
	}

	return nil
}

// validateItem validates a single order item
func (s *Service) validateItem(index int, item CreateItemRequest) error {
	if item.ProductID == "" {
		return &ItemValidationError{
			Field:   fmt.Sprintf("items[%d].product_id", index),
			Message: "product_id is required",
		}
	}

	if item.Quantity <= 0 {
		return &ItemValidationError{
			Field:   fmt.Sprintf("items[%d].quantity", index),
			Message: "quantity must be positive",
		}
	}

	if item.UnitPriceCents < 0 {
		return &ItemValidationError{
			Field:   fmt.Sprintf("items[%d].unit_price_cents", index),
			Message: "unit_price_cents cannot be negative",
		}
	}

	return nil
}

// CalculateSubtotal calculates the subtotal from items
func (s *Service) CalculateSubtotal(items []CreateItemRequest) int {
	return s.repo.CalculateSubtotal(items)
}

// ValidateItemTotals ensures each item's total matches quantity * unit_price
func (s *Service) ValidateItemTotals(items []OrderItem) error {
	for i, item := range items {
		expectedTotal := item.Quantity * item.UnitPriceCents
		if item.TotalPriceCents != expectedTotal {
			return &ItemValidationError{
				Field: fmt.Sprintf("items[%d].total_price_cents", i),
				Message: fmt.Sprintf("expected %d (quantity %d × unit_price %d), got %d",
					expectedTotal, item.Quantity, item.UnitPriceCents, item.TotalPriceCents),
			}
		}
	}
	return nil
}

// GetItemsByOrderID retrieves all items for an order
func (s *Service) GetItemsByOrderID(ctx context.Context, orderID string) ([]OrderItem, error) {
	items, err := s.repo.GetItemsByOrderID(ctx, orderID)
	if err != nil {
		s.log.Error("failed to get order items", "order_id", orderID, "error", err)
		return nil, fmt.Errorf("get order items: %w", err)
	}

	return items, nil
}

// CreateItems creates order items within a transaction
func (s *Service) CreateItems(ctx context.Context, tx pgx.Tx, orderID string, items []CreateItemRequest) ([]OrderItem, error) {
	// Validate items before creation
	if err := s.ValidateItems(items); err != nil {
		return nil, err
	}

	// Create items in database
	createdItems, err := s.repo.CreateItems(ctx, tx, orderID, items)
	if err != nil {
		s.log.Error("failed to create order items",
			"order_id", orderID,
			"items_count", len(items),
			"error", err,
		)
		return nil, fmt.Errorf("create order items: %w", err)
	}

	// Validate created items
	if err := s.ValidateItemTotals(createdItems); err != nil {
		return nil, fmt.Errorf("item total validation failed: %w", err)
	}

	s.log.Info("order items created",
		"order_id", orderID,
		"items_count", len(createdItems),
	)

	return createdItems, nil
}
