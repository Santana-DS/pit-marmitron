CREATE TABLE orders (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    public_code text NOT NULL UNIQUE,
    client_user_id uuid NOT NULL REFERENCES users(id) ON DELETE RESTRICT,
    restaurant_id uuid NOT NULL REFERENCES restaurants(id) ON DELETE RESTRICT,
    delivery_address text NOT NULL,
    status order_status NOT NULL DEFAULT 'pending',
    subtotal_cents integer NOT NULL CHECK (subtotal_cents >= 0),
    delivery_fee_cents integer NOT NULL DEFAULT 0 CHECK (delivery_fee_cents >= 0),
    discount_cents integer NOT NULL DEFAULT 0 CHECK (discount_cents >= 0),
    total_cents integer NOT NULL CHECK (total_cents >= 0),
    robot_dispatched boolean NOT NULL DEFAULT false,
    gateway_mode gateway_mode,
    mqtt_connected boolean NOT NULL DEFAULT false,
    placed_at timestamptz NOT NULL DEFAULT now(),
    dispatched_at timestamptz,
    completed_at timestamptz,
    cancelled_at timestamptz,
    cancel_reason text,
    notes text,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now(),
    CHECK (total_cents = subtotal_cents + delivery_fee_cents - discount_cents)
);

CREATE INDEX idx_orders_client ON orders(client_user_id, placed_at DESC);
CREATE INDEX idx_orders_restaurant ON orders(restaurant_id, placed_at DESC);
CREATE INDEX idx_orders_status ON orders(status);

CREATE TRIGGER orders_set_updated_at
BEFORE UPDATE ON orders
FOR EACH ROW EXECUTE FUNCTION set_updated_at();
