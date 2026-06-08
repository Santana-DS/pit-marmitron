CREATE TABLE order_items (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    order_id uuid NOT NULL REFERENCES orders(id) ON DELETE CASCADE,
    product_id uuid REFERENCES products(id) ON DELETE SET NULL,
    product_name text NOT NULL,
    product_description text NOT NULL DEFAULT '',
    product_emoji text NOT NULL DEFAULT '',
    quantity integer NOT NULL CHECK (quantity > 0),
    unit_price_cents integer NOT NULL CHECK (unit_price_cents >= 0),
    total_price_cents integer NOT NULL CHECK (total_price_cents >= 0),
    created_at timestamptz NOT NULL DEFAULT now(),
    CHECK (total_price_cents = unit_price_cents * quantity)
);

CREATE INDEX idx_order_items_order ON order_items(order_id);
