CREATE TABLE restaurants (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    owner_user_id uuid REFERENCES users(id) ON DELETE SET NULL,
    name text NOT NULL,
    emoji text NOT NULL DEFAULT '',
    bg_color char(6) NOT NULL DEFAULT 'FFFFFF',
    rating numeric(2,1) NOT NULL DEFAULT 0 CHECK (rating >= 0 AND rating <= 5),
    eta_minutes integer NOT NULL DEFAULT 0 CHECK (eta_minutes >= 0),
    is_open boolean NOT NULL DEFAULT true,
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now(),
    deleted_at timestamptz
);

CREATE INDEX idx_restaurants_open ON restaurants(is_open) WHERE deleted_at IS NULL;

CREATE TRIGGER restaurants_set_updated_at
BEFORE UPDATE ON restaurants
FOR EACH ROW EXECUTE FUNCTION set_updated_at();
