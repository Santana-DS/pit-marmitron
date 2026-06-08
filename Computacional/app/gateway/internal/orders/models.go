package orders

import (
	"time"

	"unbot-gateway/internal/order_items"
)

// Order status constants matching the database enum
const (
	StatusPending   = "pending"
	StatusPreparing = "preparing"
	StatusOnTheWay  = "on_the_way"
	StatusDelivered = "delivered"
	StatusCancelled = "cancelled"
)

// Gateway mode constants
const (
	GatewayModeFull    = "full"
	GatewayModeOTPOnly = "otp_only"
)

// Order represents a customer order
type Order struct {
	ID               string     `json:"id"`
	PublicCode       string     `json:"public_code"`
	ClientUserID     string     `json:"client_user_id"`
	RestaurantID     string     `json:"restaurant_id"`
	DeliveryAddress  string     `json:"delivery_address"`
	Status           string     `json:"status"`
	SubtotalCents    int        `json:"subtotal_cents"`
	DeliveryFeeCents int        `json:"delivery_fee_cents"`
	DiscountCents    int        `json:"discount_cents"`
	TotalCents       int        `json:"total_cents"`
	RobotDispatched  bool       `json:"robot_dispatched"`
	GatewayMode      *string    `json:"gateway_mode,omitempty"`
	MQTTConnected    bool       `json:"mqtt_connected"`
	PlacedAt         time.Time  `json:"placed_at"`
	DispatchedAt     *time.Time `json:"dispatched_at,omitempty"`
	CompletedAt      *time.Time `json:"completed_at,omitempty"`
	CancelledAt      *time.Time `json:"cancelled_at,omitempty"`
	CancelReason     *string    `json:"cancel_reason,omitempty"`
	Notes            *string    `json:"notes,omitempty"`
	CreatedAt        time.Time  `json:"created_at"`
	UpdatedAt        time.Time  `json:"updated_at"`
}

// OrderWithItems represents an order with its line items
type OrderWithItems struct {
	Order
	Items []order_items.OrderItem `json:"items"`
}

// CreateOrderRequest represents the request to create a new order
type CreateOrderRequest struct {
	ClientUserID     string            `json:"client_user_id"`
	RestaurantID     string            `json:"restaurant_id"`
	DeliveryAddress  string            `json:"delivery_address"`
	DeliveryFeeCents int               `json:"delivery_fee_cents"`
	DiscountCents    int               `json:"discount_cents"`
	Notes            *string           `json:"notes,omitempty"`
	Items            []CreateOrderItem `json:"items"`
}

// CreateOrderItem represents a line item in the create order request
type CreateOrderItem struct {
	ProductID      string `json:"product_id"`
	Quantity       int    `json:"quantity"`
	UnitPriceCents int    `json:"unit_price_cents"`
}

// UpdateOrderStatusRequest represents the request to update order status
type UpdateOrderStatusRequest struct {
	Status       string  `json:"status"`
	CancelReason *string `json:"cancel_reason,omitempty"`
}

// OrderListFilter represents filters for listing orders
type OrderListFilter struct {
	ClientUserID *string
	RestaurantID *string
	Status       *string
	Limit        int
	Offset       int
}
