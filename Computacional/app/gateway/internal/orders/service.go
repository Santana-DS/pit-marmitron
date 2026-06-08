package orders

import (
	"context"
	"fmt"
	"log/slog"

	"unbot-gateway/internal/order_items"
)

// RepositoryInterface defines the contract for order repository operations
type RepositoryInterface interface {
	CreateOrder(ctx context.Context, req CreateOrderRequest) (*OrderWithItems, error)
	GetOrderByID(ctx context.Context, orderID string) (*OrderWithItems, error)
	ListOrdersByClient(ctx context.Context, clientUserID string, limit, offset int) ([]OrderWithItems, error)
	ListOrdersByRestaurant(ctx context.Context, restaurantID string, limit, offset int) ([]OrderWithItems, error)
	UpdateOrderStatus(ctx context.Context, orderID, status string, cancelReason *string) error
}

type Service struct {
	repo     RepositoryInterface
	itemsSvc order_items.ServiceInterface
	log      *slog.Logger
}

func NewService(repo RepositoryInterface, itemsSvc order_items.ServiceInterface, log *slog.Logger) *Service {
	return &Service{
		repo:     repo,
		itemsSvc: itemsSvc,
		log:      log,
	}
}

// CreateOrder creates a new order with validation
func (s *Service) CreateOrder(ctx context.Context, req CreateOrderRequest) (*OrderWithItems, error) {
	// Validate request
	if err := s.validateCreateOrderRequest(req); err != nil {
		return nil, fmt.Errorf("validation failed: %w", err)
	}

	// Validate order total matches line items
	if err := s.validateOrderTotal(req); err != nil {
		return nil, fmt.Errorf("total validation failed: %w", err)
	}

	// Create order
	order, err := s.repo.CreateOrder(ctx, req)
	if err != nil {
		s.log.Error("failed to create order",
			"client_user_id", req.ClientUserID,
			"restaurant_id", req.RestaurantID,
			"error", err,
		)
		return nil, fmt.Errorf("create order: %w", err)
	}

	s.log.Info("order created",
		"order_id", order.ID,
		"public_code", order.PublicCode,
		"client_user_id", order.ClientUserID,
		"restaurant_id", order.RestaurantID,
		"total_cents", order.TotalCents,
		"items_count", len(order.Items),
	)

	return order, nil
}

// GetOrderByID retrieves an order by its ID
func (s *Service) GetOrderByID(ctx context.Context, orderID string) (*OrderWithItems, error) {
	order, err := s.repo.GetOrderByID(ctx, orderID)
	if err != nil {
		if err == ErrOrderNotFound {
			return nil, err
		}
		s.log.Error("failed to get order", "order_id", orderID, "error", err)
		return nil, fmt.Errorf("get order: %w", err)
	}

	return order, nil
}

// ListOrdersByClient retrieves orders for a specific client
func (s *Service) ListOrdersByClient(ctx context.Context, clientUserID string, limit, offset int) ([]OrderWithItems, error) {
	// Set default limit if not provided
	if limit <= 0 {
		limit = 20
	}
	if limit > 100 {
		limit = 100
	}

	orders, err := s.repo.ListOrdersByClient(ctx, clientUserID, limit, offset)
	if err != nil {
		s.log.Error("failed to list orders",
			"client_user_id", clientUserID,
			"error", err,
		)
		return nil, fmt.Errorf("list orders: %w", err)
	}

	return orders, nil
}

// ListOrdersByRestaurant retrieves orders for a specific restaurant
func (s *Service) ListOrdersByRestaurant(ctx context.Context, restaurantID string, limit, offset int) ([]OrderWithItems, error) {
	// Set default limit if not provided
	if limit <= 0 {
		limit = 20
	}
	if limit > 100 {
		limit = 100
	}

	orders, err := s.repo.ListOrdersByRestaurant(ctx, restaurantID, limit, offset)
	if err != nil {
		s.log.Error("failed to list restaurant orders",
			"restaurant_id", restaurantID,
			"error", err,
		)
		return nil, fmt.Errorf("list restaurant orders: %w", err)
	}

	return orders, nil
}

// UpdateOrderStatus updates the status of an order with validation
func (s *Service) UpdateOrderStatus(ctx context.Context, orderID string, req UpdateOrderStatusRequest) error {
	// Validate status transition
	if err := s.validateStatusTransition(ctx, orderID, req.Status); err != nil {
		return fmt.Errorf("invalid status transition: %w", err)
	}

	// Update status
	if err := s.repo.UpdateOrderStatus(ctx, orderID, req.Status, req.CancelReason); err != nil {
		if err == ErrOrderNotFound {
			return err
		}
		s.log.Error("failed to update order status",
			"order_id", orderID,
			"new_status", req.Status,
			"error", err,
		)
		return fmt.Errorf("update order status: %w", err)
	}

	s.log.Info("order status updated",
		"order_id", orderID,
		"new_status", req.Status,
	)

	return nil
}

// validateCreateOrderRequest validates the create order request
func (s *Service) validateCreateOrderRequest(req CreateOrderRequest) error {
	if req.ClientUserID == "" {
		return fmt.Errorf("client_user_id is required")
	}
	if req.RestaurantID == "" {
		return fmt.Errorf("restaurant_id is required")
	}
	if req.DeliveryAddress == "" {
		return fmt.Errorf("delivery_address is required")
	}
	if req.DeliveryFeeCents < 0 {
		return fmt.Errorf("delivery_fee_cents cannot be negative")
	}
	if req.DiscountCents < 0 {
		return fmt.Errorf("discount_cents cannot be negative")
	}

	// Convert items to order_items format for validation
	itemRequests := make([]order_items.CreateItemRequest, len(req.Items))
	for i, item := range req.Items {
		itemRequests[i] = order_items.CreateItemRequest{
			ProductID:      item.ProductID,
			Quantity:       item.Quantity,
			UnitPriceCents: item.UnitPriceCents,
		}
	}

	// Validate items using order_items service
	if err := s.itemsSvc.ValidateItems(itemRequests); err != nil {
		return err
	}

	return nil
}

// validateOrderTotal validates that the order total matches the sum of line items
func (s *Service) validateOrderTotal(req CreateOrderRequest) error {
	// Convert items to order_items format
	itemRequests := make([]order_items.CreateItemRequest, len(req.Items))
	for i, item := range req.Items {
		itemRequests[i] = order_items.CreateItemRequest{
			ProductID:      item.ProductID,
			Quantity:       item.Quantity,
			UnitPriceCents: item.UnitPriceCents,
		}
	}

	// Calculate expected subtotal using items service
	calculatedSubtotal := s.itemsSvc.CalculateSubtotal(itemRequests)

	// Calculate expected total
	expectedTotal := calculatedSubtotal + req.DeliveryFeeCents - req.DiscountCents

	// Ensure total is not negative
	if expectedTotal < 0 {
		return fmt.Errorf("order total cannot be negative (subtotal: %d, delivery: %d, discount: %d)",
			calculatedSubtotal, req.DeliveryFeeCents, req.DiscountCents)
	}

	s.log.Debug("order total validated",
		"calculated_subtotal", calculatedSubtotal,
		"delivery_fee", req.DeliveryFeeCents,
		"discount", req.DiscountCents,
		"expected_total", expectedTotal,
	)

	return nil
}

// validateStatusTransition validates that a status transition is allowed
func (s *Service) validateStatusTransition(ctx context.Context, orderID, newStatus string) error {
	// Get current order
	order, err := s.repo.GetOrderByID(ctx, orderID)
	if err != nil {
		return err
	}

	currentStatus := order.Status

	// Define valid transitions
	validTransitions := map[string][]string{
		StatusPending: {
			StatusPreparing,
			StatusCancelled,
		},
		StatusPreparing: {
			StatusOnTheWay,
			StatusCancelled,
		},
		StatusOnTheWay: {
			StatusDelivered,
			StatusCancelled,
		},
		StatusDelivered: {
			// Terminal state - no transitions allowed
		},
		StatusCancelled: {
			// Terminal state - no transitions allowed
		},
	}

	// Check if transition is valid
	allowedStatuses, exists := validTransitions[currentStatus]
	if !exists {
		return fmt.Errorf("unknown current status: %s", currentStatus)
	}

	// Check if new status is in allowed list
	for _, allowed := range allowedStatuses {
		if newStatus == allowed {
			return nil
		}
	}

	return fmt.Errorf("cannot transition from %s to %s", currentStatus, newStatus)
}
