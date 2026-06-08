CREATE EXTENSION IF NOT EXISTS citext;

INSERT INTO users (
    id,
    role,
    name,
    email,
    phone,
    default_address
) VALUES
    ('11111111-1111-1111-1111-111111111111', 'client', 'Maria Silva', 'maria.unb@gmail.com', '(61) 99999-1234', 'Rua das Acacias, 42 - Asa Sul'),
    ('22222222-2222-2222-2222-222222222222', 'restaurant_admin', 'Marmitas da Vo Admin', 'admin@marmitasdavo.test', NULL, NULL)
ON CONFLICT (id) DO NOTHING;

INSERT INTO restaurants (
    id,
    owner_user_id,
    name,
    emoji,
    bg_color,
    rating,
    eta_minutes,
    is_open
) VALUES
    ('aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1', '22222222-2222-2222-2222-222222222222', 'Marmitas da Vo', '🍱', 'FFF3EE', 4.8, 12, true),
    ('aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa2', NULL, 'Fit & Fresh', '🥗', 'EDFCF8', 4.6, 18, true),
    ('aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa3', NULL, 'Pasta & Co.', '🍝', 'F0F4FF', 4.9, 22, true),
    ('aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa4', NULL, 'Frango Grelhado', '🍗', 'FFF8EE', 4.7, 15, true)
ON CONFLICT (id) DO NOTHING;

INSERT INTO products (
    id,
    restaurant_id,
    name,
    description,
    emoji,
    price_cents,
    is_available,
    sort_order
) VALUES
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb001', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1', 'Marmita Executiva', 'Frango grelhado, arroz, feijao, salada', '🍱', 1800, true, 1),
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb002', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1', 'Marmita Vegana', 'Legumes grelhados, arroz integral, grao de bico', '🥗', 2000, true, 2),
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb003', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1', 'Marmita Especial', 'Bife grelhado, pure, farofa, arroz e feijao', '🍖', 2400, true, 3),
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb004', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa2', 'Bowl Fitness', 'Quinoa, legumes, peito de peru, molho', '🥗', 2200, true, 1),
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb005', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa2', 'Salada Caesar', 'Frango, croutons, parmesao, molho caesar', '🥙', 1900, true, 2),
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb006', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa3', 'Massa ao Molho', 'Espaguete, molho pomodoro, parmesao', '🍝', 2000, true, 1),
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb007', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa3', 'Carbonara', 'Espaguete, bacon, ovos, parmesao', '🍜', 2300, true, 2),
    ('bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb008', 'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa4', 'Meio Frango', 'Frango grelhado temperado, acompanhamentos', '🍗', 2800, true, 1)
ON CONFLICT (id) DO NOTHING;
