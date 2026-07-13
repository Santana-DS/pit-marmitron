package api

import (
	"encoding/json"
	"errors"
	"net/http"
	"strconv"
	"strings"

	"unbot-gateway/internal/orders"
)

// TODO: TECHNICAL DEBT - Mock client_user_id
// This is a temporary solution until user authentication is implemented.
// When user management is complete, this should be replaced with actual
// authenticated user ID from JWT token or session.
// Using test user "Maria Silva" from seed data (001_sample_data.sql)
const mockClientUserID = "11111111-1111-1111-1111-111111111111"

type createOrderResponse struct {
	Order *orders.OrderWithItems `json:"order"`
}

type listOrdersResponse struct {
	Orders []orders.OrderWithItems `json:"orders"`
	Total  int                     `json:"total"`
	Limit  int                     `json:"limit"`
	Offset int                     `json:"offset"`
}

type updateOrderStatusResponse struct {
	Success bool   `json:"success"`
	OrderID string `json:"order_id"`
	Status  string `json:"status"`
}

// createOrderHandler handles POST /api/orders
func (s *Server) createOrderHandler(orderSvc *orders.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		var req orders.CreateOrderRequest
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid request body"})
			return
		}

		// TODO: TECHNICAL DEBT - Remove this when user authentication is implemented
		// If client_user_id is not provided, use mock value for testing
		if req.ClientUserID == "" {
			req.ClientUserID = mockClientUserID
			s.log.Warn("using mock client_user_id - implement user authentication",
				"mock_id", mockClientUserID)
		}

		order, err := orderSvc.CreateOrder(r.Context(), req)
		if err != nil {
			if strings.Contains(err.Error(), "validation failed") {
				writeJSON(w, http.StatusBadRequest, errorResponse{Error: err.Error()})
				return
			}
			if strings.Contains(err.Error(), "total validation failed") {
				writeJSON(w, http.StatusBadRequest, errorResponse{Error: err.Error()})
				return
			}

			s.log.Error("create order failed", "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusCreated, createOrderResponse{Order: order})
	}
}

// getOrderHandler handles GET /api/orders/{id}
func (s *Server) getOrderHandler(orderSvc *orders.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		orderID := strings.TrimSpace(r.PathValue("id"))
		if orderID == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "order id path parameter is required"})
			return
		}

		order, err := orderSvc.GetOrderByID(r.Context(), orderID)
		if err != nil {
			if errors.Is(err, orders.ErrOrderNotFound) {
				writeJSON(w, http.StatusNotFound, errorResponse{Error: "order not found"})
				return
			}

			s.log.Error("get order failed", "order_id", orderID, "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusOK, order)
	}
}

// listOrdersByClientHandler handles GET /api/clients/{client_id}/orders
func (s *Server) listOrdersByClientHandler(orderSvc *orders.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		clientID := strings.TrimSpace(r.PathValue("client_id"))
		if clientID == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "client id path parameter is required"})
			return
		}

		// Parse query parameters
		limitStr := r.URL.Query().Get("limit")
		offsetStr := r.URL.Query().Get("offset")

		limit := 20 // default
		if limitStr != "" {
			if l, err := strconv.Atoi(limitStr); err == nil && l > 0 {
				limit = l
			}
		}

		offset := 0 // default
		if offsetStr != "" {
			if o, err := strconv.Atoi(offsetStr); err == nil && o >= 0 {
				offset = o
			}
		}

		ordersList, err := orderSvc.ListOrdersByClient(r.Context(), clientID, limit, offset)
		if err != nil {
			s.log.Error("list orders failed", "client_id", clientID, "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusOK, listOrdersResponse{
			Orders: ordersList,
			Total:  len(ordersList),
			Limit:  limit,
			Offset: offset,
		})
	}
}

// listOrdersByRestaurantHandler handles GET /api/restaurants/{id}/orders
func (s *Server) listOrdersByRestaurantHandler(orderSvc *orders.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		restaurantID := strings.TrimSpace(r.PathValue("id"))
		if restaurantID == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "restaurant id path parameter is required"})
			return
		}

		limitStr := r.URL.Query().Get("limit")
		offsetStr := r.URL.Query().Get("offset")

		limit := 20
		if limitStr != "" {
			if l, err := strconv.Atoi(limitStr); err == nil && l > 0 {
				limit = l
			}
		}

		offset := 0
		if offsetStr != "" {
			if o, err := strconv.Atoi(offsetStr); err == nil && o >= 0 {
				offset = o
			}
		}

		ordersList, err := orderSvc.ListOrdersByRestaurant(r.Context(), restaurantID, limit, offset)
		if err != nil {
			s.log.Error("list restaurant orders failed", "restaurant_id", restaurantID, "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusOK, listOrdersResponse{
			Orders: ordersList,
			Total:  len(ordersList),
			Limit:  limit,
			Offset: offset,
		})
	}
}

// updateOrderStatusHandler handles PATCH /api/orders/{id}/status
func (s *Server) updateOrderStatusHandler(orderSvc *orders.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		orderID := strings.TrimSpace(r.PathValue("id"))
		if orderID == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "order id path parameter is required"})
			return
		}

		var req orders.UpdateOrderStatusRequest
		if err := json.NewDecoder(r.Body).Decode(&req); err != nil {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid request body"})
			return
		}

		if req.Status == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "status is required"})
			return
		}

		err := orderSvc.UpdateOrderStatus(r.Context(), orderID, req)
		if err != nil {
			if errors.Is(err, orders.ErrOrderNotFound) {
				writeJSON(w, http.StatusNotFound, errorResponse{Error: "order not found"})
				return
			}
			if errors.Is(err, orders.ErrInvalidStatus) {
				writeJSON(w, http.StatusBadRequest, errorResponse{Error: "invalid status"})
				return
			}
			if strings.Contains(err.Error(), "invalid status transition") {
				writeJSON(w, http.StatusBadRequest, errorResponse{Error: err.Error()})
				return
			}
			if errors.Is(err, orders.ErrCancelCommandPublish) {
				writeJSON(w, http.StatusBadGateway, errorResponse{Error: "navigation cancellation could not be delivered to robot"})
				return
			}

			s.log.Error("update order status failed", "order_id", orderID, "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusOK, updateOrderStatusResponse{
			Success: true,
			OrderID: orderID,
			Status:  req.Status,
		})
	}
}
