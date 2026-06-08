package order_items

import "time"

// OrderItem represents a line item in an order
type OrderItem struct {
	ID                 string    `json:"id"`
	OrderID            string    `json:"order_id"`
	ProductID          *string   `json:"product_id,omitempty"`
	ProductName        string    `json:"product_name"`
	ProductDescription string    `json:"product_description"`
	ProductEmoji       string    `json:"product_emoji"`
	Quantity           int       `json:"quantity"`
	UnitPriceCents     int       `json:"unit_price_cents"`
	TotalPriceCents    int       `json:"total_price_cents"`
	CreatedAt          time.Time `json:"created_at"`
}

// CreateItemRequest represents a request to create an order item
type CreateItemRequest struct {
	OrderID        string `json:"order_id"`
	ProductID      string `json:"product_id"`
	Quantity       int    `json:"quantity"`
	UnitPriceCents int    `json:"unit_price_cents"`
}

// ItemValidationError represents validation errors for order items
type ItemValidationError struct {
	Field   string
	Message string
}

func (e *ItemValidationError) Error() string {
	return e.Field + ": " + e.Message
}
