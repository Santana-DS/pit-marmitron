package orders

import (
	"context"
	"log/slog"
	"os"
	"testing"

	"unbot-gateway/internal/order_items"

	"github.com/jackc/pgx/v5"
)

func TestValidateCreateOrderRequest(t *testing.T) {
	log := slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelError}))
	mockItemsSvc := &mockItemsService{}
	svc := &Service{itemsSvc: mockItemsSvc, log: log}

	tests := []struct {
		name    string
		req     CreateOrderRequest
		wantErr bool
		errMsg  string
	}{
		{
			name: "valid request",
			req: CreateOrderRequest{
				ClientUserID:     "client-123",
				RestaurantID:     "restaurant-456",
				DeliveryAddress:  "123 Main St",
				DeliveryFeeCents: 500,
				DiscountCents:    0,
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 2, UnitPriceCents: 1000},
				},
			},
			wantErr: false,
		},
		{
			name: "missing client_user_id",
			req: CreateOrderRequest{
				RestaurantID:    "restaurant-456",
				DeliveryAddress: "123 Main St",
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: 1000},
				},
			},
			wantErr: true,
			errMsg:  "client_user_id is required",
		},
		{
			name: "missing restaurant_id",
			req: CreateOrderRequest{
				ClientUserID:    "client-123",
				DeliveryAddress: "123 Main St",
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: 1000},
				},
			},
			wantErr: true,
			errMsg:  "restaurant_id is required",
		},
		{
			name: "missing delivery_address",
			req: CreateOrderRequest{
				ClientUserID: "client-123",
				RestaurantID: "restaurant-456",
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: 1000},
				},
			},
			wantErr: true,
			errMsg:  "delivery_address is required",
		},
		{
			name: "no items",
			req: CreateOrderRequest{
				ClientUserID:    "client-123",
				RestaurantID:    "restaurant-456",
				DeliveryAddress: "123 Main St",
				Items:           []CreateOrderItem{},
			},
			wantErr: true,
			errMsg:  "items: at least one item is required",
		},
		{
			name: "negative delivery fee",
			req: CreateOrderRequest{
				ClientUserID:     "client-123",
				RestaurantID:     "restaurant-456",
				DeliveryAddress:  "123 Main St",
				DeliveryFeeCents: -100,
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: 1000},
				},
			},
			wantErr: true,
			errMsg:  "delivery_fee_cents cannot be negative",
		},
		{
			name: "negative discount",
			req: CreateOrderRequest{
				ClientUserID:    "client-123",
				RestaurantID:    "restaurant-456",
				DeliveryAddress: "123 Main St",
				DiscountCents:   -100,
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: 1000},
				},
			},
			wantErr: true,
			errMsg:  "discount_cents cannot be negative",
		},
		{
			name: "item with zero quantity",
			req: CreateOrderRequest{
				ClientUserID:    "client-123",
				RestaurantID:    "restaurant-456",
				DeliveryAddress: "123 Main St",
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 0, UnitPriceCents: 1000},
				},
			},
			wantErr: true,
			errMsg:  "items[0].quantity: quantity must be positive",
		},
		{
			name: "item with negative price",
			req: CreateOrderRequest{
				ClientUserID:    "client-123",
				RestaurantID:    "restaurant-456",
				DeliveryAddress: "123 Main St",
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: -1000},
				},
			},
			wantErr: true,
			errMsg:  "items[0].unit_price_cents: unit_price_cents cannot be negative",
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := svc.validateCreateOrderRequest(tt.req)
			if (err != nil) != tt.wantErr {
				t.Errorf("validateCreateOrderRequest() error = %v, wantErr %v", err, tt.wantErr)
				return
			}
			if tt.wantErr && err.Error() != tt.errMsg {
				t.Errorf("validateCreateOrderRequest() error message = %v, want %v", err.Error(), tt.errMsg)
			}
		})
	}
}

func TestValidateOrderTotal(t *testing.T) {
	log := slog.New(slog.NewTextHandler(os.Stdout, &slog.HandlerOptions{Level: slog.LevelError}))
	mockItemsSvc := &mockItemsService{}
	svc := &Service{itemsSvc: mockItemsSvc, log: log}

	tests := []struct {
		name    string
		req     CreateOrderRequest
		wantErr bool
	}{
		{
			name: "valid total calculation",
			req: CreateOrderRequest{
				DeliveryFeeCents: 500,
				DiscountCents:    100,
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 2, UnitPriceCents: 1000}, // 2000
					{ProductID: "prod-2", Quantity: 1, UnitPriceCents: 500},  // 500
				},
				// Subtotal: 2500, Delivery: 500, Discount: 100 = Total: 2900
			},
			wantErr: false,
		},
		{
			name: "negative total",
			req: CreateOrderRequest{
				DeliveryFeeCents: 100,
				DiscountCents:    5000,
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: 1000}, // 1000
				},
				// Subtotal: 1000, Delivery: 100, Discount: 5000 = Total: -3900 (invalid)
			},
			wantErr: true,
		},
		{
			name: "zero total is valid",
			req: CreateOrderRequest{
				DeliveryFeeCents: 0,
				DiscountCents:    1000,
				Items: []CreateOrderItem{
					{ProductID: "prod-1", Quantity: 1, UnitPriceCents: 1000}, // 1000
				},
				// Subtotal: 1000, Delivery: 0, Discount: 1000 = Total: 0 (valid)
			},
			wantErr: false,
		},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			err := svc.validateOrderTotal(tt.req)
			if (err != nil) != tt.wantErr {
				t.Errorf("validateOrderTotal() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

func TestValidateStatusTransition(t *testing.T) {
	tests := []struct {
		name          string
		currentStatus string
		newStatus     string
		wantErr       bool
	}{
		// Valid transitions
		{name: "pending to preparing", currentStatus: StatusPending, newStatus: StatusPreparing, wantErr: false},
		{name: "pending to cancelled", currentStatus: StatusPending, newStatus: StatusCancelled, wantErr: false},
		{name: "preparing to on_the_way", currentStatus: StatusPreparing, newStatus: StatusOnTheWay, wantErr: false},
		{name: "preparing to cancelled", currentStatus: StatusPreparing, newStatus: StatusCancelled, wantErr: false},
		{name: "on_the_way to delivered", currentStatus: StatusOnTheWay, newStatus: StatusDelivered, wantErr: false},
		{name: "on_the_way to cancelled", currentStatus: StatusOnTheWay, newStatus: StatusCancelled, wantErr: false},

		// Invalid transitions
		{name: "pending to delivered", currentStatus: StatusPending, newStatus: StatusDelivered, wantErr: true},
		{name: "pending to on_the_way", currentStatus: StatusPending, newStatus: StatusOnTheWay, wantErr: true},
		{name: "preparing to delivered", currentStatus: StatusPreparing, newStatus: StatusDelivered, wantErr: true},
		{name: "delivered to cancelled", currentStatus: StatusDelivered, newStatus: StatusCancelled, wantErr: true},
		{name: "cancelled to preparing", currentStatus: StatusCancelled, newStatus: StatusPreparing, wantErr: true},
		{name: "delivered to preparing", currentStatus: StatusDelivered, newStatus: StatusPreparing, wantErr: true},
	}

	for _, tt := range tests {
		t.Run(tt.name, func(t *testing.T) {
			// Create a mock order
			mockOrder := &OrderWithItems{
				Order: Order{
					ID:     "test-order-id",
					Status: tt.currentStatus,
				},
			}

			// Create a mock repository that returns our mock order
			mockRepo := &mockRepository{order: mockOrder}
			svc := &Service{repo: mockRepo}

			err := svc.validateStatusTransition(context.Background(), "test-order-id", tt.newStatus)
			if (err != nil) != tt.wantErr {
				t.Errorf("validateStatusTransition() error = %v, wantErr %v", err, tt.wantErr)
			}
		})
	}
}

// mockRepository is a simple mock for testing
type mockRepository struct {
	order *OrderWithItems
	err   error
}

func (m *mockRepository) GetOrderByID(ctx context.Context, orderID string) (*OrderWithItems, error) {
	if m.err != nil {
		return nil, m.err
	}
	return m.order, nil
}

func (m *mockRepository) CreateOrder(ctx context.Context, req CreateOrderRequest) (*OrderWithItems, error) {
	return nil, nil
}

func (m *mockRepository) ListOrdersByClient(ctx context.Context, clientUserID string, limit, offset int) ([]OrderWithItems, error) {
	return nil, nil
}

func (m *mockRepository) ListOrdersByRestaurant(ctx context.Context, restaurantID string, limit, offset int) ([]OrderWithItems, error) {
	return nil, nil
}

func (m *mockRepository) UpdateOrderStatus(ctx context.Context, orderID, status string, cancelReason *string) error {
	return nil
}

// mockItemsService is a simple mock for order items service
type mockItemsService struct{}

// Ensure mockItemsService implements ServiceInterface
var _ order_items.ServiceInterface = (*mockItemsService)(nil)

func (m *mockItemsService) ValidateItems(items []order_items.CreateItemRequest) error {
	if len(items) == 0 {
		return &order_items.ItemValidationError{
			Field:   "items",
			Message: "at least one item is required",
		}
	}

	for i, item := range items {
		if item.ProductID == "" {
			return &order_items.ItemValidationError{
				Field:   "product_id",
				Message: "product_id is required",
			}
		}
		if item.Quantity <= 0 {
			return &order_items.ItemValidationError{
				Field:   "items[" + string(rune(i+'0')) + "].quantity",
				Message: "quantity must be positive",
			}
		}
		if item.UnitPriceCents < 0 {
			return &order_items.ItemValidationError{
				Field:   "items[" + string(rune(i+'0')) + "].unit_price_cents",
				Message: "unit_price_cents cannot be negative",
			}
		}
	}
	return nil
}

func (m *mockItemsService) CalculateSubtotal(items []order_items.CreateItemRequest) int {
	subtotal := 0
	for _, item := range items {
		subtotal += item.UnitPriceCents * item.Quantity
	}
	return subtotal
}

func (m *mockItemsService) GetItemsByOrderID(ctx context.Context, orderID string) ([]order_items.OrderItem, error) {
	return nil, nil
}

func (m *mockItemsService) CreateItems(ctx context.Context, tx pgx.Tx, orderID string, items []order_items.CreateItemRequest) ([]order_items.OrderItem, error) {
	return nil, nil
}

func (m *mockItemsService) ValidateItemTotals(items []order_items.OrderItem) error {
	return nil
}
