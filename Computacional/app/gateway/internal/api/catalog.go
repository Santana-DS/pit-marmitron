package api

import (
	"errors"
	"net/http"
	"strings"

	"unbot-gateway/internal/catalog"
)

type listRestaurantsResponse struct {
	Restaurants []catalog.Restaurant `json:"restaurants"`
}

type listProductsResponse struct {
	Products []catalog.Product `json:"products"`
}

func (s *Server) listRestaurantsHandler(catalogSvc *catalog.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		query := strings.TrimSpace(r.URL.Query().Get("q"))

		items, err := catalogSvc.ListRestaurants(r.Context(), query)
		if err != nil {
			s.log.Error("list restaurants failed", "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusOK, listRestaurantsResponse{
			Restaurants: items,
		})
	}
}

func (s *Server) getRestaurantHandler(catalogSvc *catalog.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		restaurantID := strings.TrimSpace(r.PathValue("id"))
		if restaurantID == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "restaurant id path parameter is required"})
			return
		}

		item, err := catalogSvc.GetRestaurantDetails(r.Context(), restaurantID)
		if err != nil {
			if strings.Contains(err.Error(), catalog.ErrRestaurantNotFound.Error()) || errors.Is(err, catalog.ErrRestaurantNotFound) {
				writeJSON(w, http.StatusNotFound, errorResponse{Error: "restaurant not found"})
				return
			}

			s.log.Error("get restaurant failed", "restaurant_id", restaurantID, "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusOK, item)
	}
}

func (s *Server) listRestaurantProductsHandler(catalogSvc *catalog.Service) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) {
		restaurantID := strings.TrimSpace(r.PathValue("id"))
		if restaurantID == "" {
			writeJSON(w, http.StatusBadRequest, errorResponse{Error: "restaurant id path parameter is required"})
			return
		}

		items, err := catalogSvc.ListProductsByRestaurant(r.Context(), restaurantID)
		if err != nil {
			if strings.Contains(err.Error(), catalog.ErrRestaurantNotFound.Error()) || errors.Is(err, catalog.ErrRestaurantNotFound) {
				writeJSON(w, http.StatusNotFound, errorResponse{Error: "restaurant not found"})
				return
			}

			s.log.Error("list restaurant products failed", "restaurant_id", restaurantID, "error", err)
			writeJSON(w, http.StatusInternalServerError, errorResponse{Error: "internal server error"})
			return
		}

		writeJSON(w, http.StatusOK, listProductsResponse{
			Products: items,
		})
	}
}
