package catalog

import (
	"errors"
	"testing"
)

func TestNewService(t *testing.T) {
	repo := &Repository{}
	svc := NewService(repo)

	if svc == nil {
		t.Fatal("expected non-nil service")
	}
	if svc.repo != repo {
		t.Fatal("expected service to keep repository reference")
	}
}

func TestErrRestaurantNotFound_IsComparable(t *testing.T) {
	if !errors.Is(ErrRestaurantNotFound, ErrRestaurantNotFound) {
		t.Fatal("expected ErrRestaurantNotFound to match itself")
	}
}
