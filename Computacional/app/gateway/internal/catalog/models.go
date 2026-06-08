package catalog

type Restaurant struct {
	ID         string  `json:"id"`
	Name       string  `json:"name"`
	Emoji      string  `json:"emoji"`
	BgColor    string  `json:"bg_color"`
	Rating     float64 `json:"rating"`
	ETAMinutes int     `json:"eta_minutes"`
	IsOpen     bool    `json:"is_open"`
}

type Product struct {
	ID           string `json:"id"`
	RestaurantID string `json:"restaurant_id"`
	Name         string `json:"name"`
	Description  string `json:"description"`
	Emoji        string `json:"emoji"`
	PriceCents   int    `json:"price_cents"`
	IsAvailable  bool   `json:"is_available"`
	SortOrder    int    `json:"sort_order"`
}

type RestaurantDetails struct {
	Restaurant
	Products []Product `json:"products"`
}
