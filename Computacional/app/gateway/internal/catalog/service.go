package catalog

import (
	"context"
	"fmt"
	"strings"
)

type Service struct {
	repo *Repository
}

func NewService(repo *Repository) *Service {
	return &Service{repo: repo}
}

func (s *Service) ListRestaurants(ctx context.Context, query string) ([]Restaurant, error) {
	normalized := strings.TrimSpace(query)
	items, err := s.repo.ListRestaurants(ctx, normalized)
	if err != nil {
		return nil, fmt.Errorf("list restaurants: %w", err)
	}
	return items, nil
}

func (s *Service) GetRestaurantDetails(ctx context.Context, restaurantID string) (*RestaurantDetails, error) {
	restaurantID = strings.TrimSpace(restaurantID)

	restaurant, err := s.repo.GetRestaurantByID(ctx, restaurantID)
	if err != nil {
		return nil, fmt.Errorf("get restaurant details: %w", err)
	}

	products, err := s.repo.ListProductsByRestaurant(ctx, restaurantID)
	if err != nil {
		return nil, fmt.Errorf("list products by restaurant: %w", err)
	}

	return &RestaurantDetails{
		Restaurant: *restaurant,
		Products:   products,
	}, nil
}

func (s *Service) ListProductsByRestaurant(ctx context.Context, restaurantID string) ([]Product, error) {
	restaurantID = strings.TrimSpace(restaurantID)

	if _, err := s.repo.GetRestaurantByID(ctx, restaurantID); err != nil {
		return nil, fmt.Errorf("check restaurant existence: %w", err)
	}

	products, err := s.repo.ListProductsByRestaurant(ctx, restaurantID)
	if err != nil {
		return nil, fmt.Errorf("list products by restaurant: %w", err)
	}

	return products, nil
}
