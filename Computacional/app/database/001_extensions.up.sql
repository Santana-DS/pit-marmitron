CREATE EXTENSION IF NOT EXISTS pgcrypto;
CREATE EXTENSION IF NOT EXISTS citext;

CREATE TYPE user_role AS ENUM ('client', 'restaurant_admin', 'operator');
CREATE TYPE order_status AS ENUM (
    'pending',
    'preparing',
    'on_the_way',
    'delivered',
    'cancelled'
);
CREATE TYPE otp_status AS ENUM ('active', 'used', 'expired', 'revoked');
CREATE TYPE robot_command_type AS ENUM ('navigate', 'unlock');
CREATE TYPE robot_command_status AS ENUM ('queued', 'published', 'failed', 'acknowledged');
CREATE TYPE gateway_mode AS ENUM ('full', 'otp_only');
