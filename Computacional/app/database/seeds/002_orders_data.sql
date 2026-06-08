-- Seed data for orders and order_items tables
-- This creates sample orders with different statuses for testing

-- Insert sample orders for Maria Silva (client)
INSERT INTO orders (
    id,
    public_code,
    client_user_id,
    restaurant_id,
    delivery_address,
    status,
    subtotal_cents,
    delivery_fee_cents,
    discount_cents,
    total_cents,
    robot_dispatched,
    gateway_mode,
    mqtt_connected,
    placed_at,
    dispatched_at,
    completed_at,
    notes
) VALUES
    -- Order 1: Completed order from Marmitas da Vo
    (
        'cccccccc-cccc-cccc-cccc-cccccccccc01',
        '123456',
        '11111111-1111-1111-1111-111111111111',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1',
        'Rua das Acacias, 42 - Asa Sul',
        'delivered',
        4200, -- subtotal: 1800 + 2400
        500,  -- delivery fee
        0,    -- no discount
        4700, -- total
        true,
        'full',
        true,
        NOW() - INTERVAL '2 hours',
        NOW() - INTERVAL '1 hour 45 minutes',
        NOW() - INTERVAL '1 hour 30 minutes',
        'Deixar na portaria'
    ),
    
    -- Order 2: On the way order from Fit & Fresh
    (
        'cccccccc-cccc-cccc-cccc-cccccccccc02',
        '234567',
        '11111111-1111-1111-1111-111111111111',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa2',
        'Rua das Acacias, 42 - Asa Sul',
        'on_the_way',
        4100, -- subtotal: 2200 + 1900
        500,
        200,  -- discount
        4400,
        true,
        'full',
        true,
        NOW() - INTERVAL '30 minutes',
        NOW() - INTERVAL '10 minutes',
        NULL,
        NULL
    ),
    
    -- Order 3: Preparing order from Pasta & Co.
    (
        'cccccccc-cccc-cccc-cccc-cccccccccc03',
        '345678',
        '11111111-1111-1111-1111-111111111111',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa3',
        'Rua das Acacias, 42 - Asa Sul',
        'preparing',
        4300, -- subtotal: 2000 + 2300
        500,
        0,
        4800,
        false,
        NULL,
        false,
        NOW() - INTERVAL '15 minutes',
        NULL,
        NULL,
        'Sem cebola por favor'
    ),
    
    -- Order 4: Pending order from Frango Grelhado
    (
        'cccccccc-cccc-cccc-cccc-cccccccccc04',
        '456789',
        '11111111-1111-1111-1111-111111111111',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa4',
        'Rua das Acacias, 42 - Asa Sul',
        'pending',
        2800,
        500,
        0,
        3300,
        false,
        NULL,
        false,
        NOW() - INTERVAL '5 minutes',
        NULL,
        NULL,
        NULL
    ),
    
    -- Order 5: Cancelled order from Marmitas da Vo
    (
        'cccccccc-cccc-cccc-cccc-cccccccccc05',
        '567890',
        '11111111-1111-1111-1111-111111111111',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa1',
        'Rua das Acacias, 42 - Asa Sul',
        'cancelled',
        2000,
        500,
        0,
        2500,
        false,
        NULL,
        false,
        NOW() - INTERVAL '3 hours',
        NULL,
        NULL,
        'Cliente cancelou'
    ),
    
    -- Order 6: Another completed order (older)
    (
        'cccccccc-cccc-cccc-cccc-cccccccccc06',
        '678901',
        '11111111-1111-1111-1111-111111111111',
        'aaaaaaaa-aaaa-aaaa-aaaa-aaaaaaaaaaa2',
        'Rua das Acacias, 42 - Asa Sul',
        'delivered',
        2200,
        500,
        100,
        2600,
        true,
        'full',
        true,
        NOW() - INTERVAL '1 day',
        NOW() - INTERVAL '23 hours 45 minutes',
        NOW() - INTERVAL '23 hours 30 minutes',
        NULL
    )
ON CONFLICT (id) DO NOTHING;

-- Insert order items for each order
INSERT INTO order_items (
    id,
    order_id,
    product_id,
    product_name,
    product_description,
    product_emoji,
    quantity,
    unit_price_cents,
    total_price_cents
) VALUES
    -- Order 1 items (Marmitas da Vo)
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd01',
        'cccccccc-cccc-cccc-cccc-cccccccccc01',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb001',
        'Marmita Executiva',
        'Frango grelhado, arroz, feijao, salada',
        '🍱',
        1,
        1800,
        1800
    ),
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd02',
        'cccccccc-cccc-cccc-cccc-cccccccccc01',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb003',
        'Marmita Especial',
        'Bife grelhado, pure, farofa, arroz e feijao',
        '🍖',
        1,
        2400,
        2400
    ),
    
    -- Order 2 items (Fit & Fresh)
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd03',
        'cccccccc-cccc-cccc-cccc-cccccccccc02',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb004',
        'Bowl Fitness',
        'Quinoa, legumes, peito de peru, molho',
        '🥗',
        1,
        2200,
        2200
    ),
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd04',
        'cccccccc-cccc-cccc-cccc-cccccccccc02',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb005',
        'Salada Caesar',
        'Frango, croutons, parmesao, molho caesar',
        '🥙',
        1,
        1900,
        1900
    ),
    
    -- Order 3 items (Pasta & Co.)
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd05',
        'cccccccc-cccc-cccc-cccc-cccccccccc03',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb006',
        'Massa ao Molho',
        'Espaguete, molho pomodoro, parmesao',
        '🍝',
        1,
        2000,
        2000
    ),
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd06',
        'cccccccc-cccc-cccc-cccc-cccccccccc03',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb007',
        'Carbonara',
        'Espaguete, bacon, ovos, parmesao',
        '🍜',
        1,
        2300,
        2300
    ),
    
    -- Order 4 items (Frango Grelhado)
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd07',
        'cccccccc-cccc-cccc-cccc-cccccccccc04',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb008',
        'Meio Frango',
        'Frango grelhado temperado, acompanhamentos',
        '🍗',
        1,
        2800,
        2800
    ),
    
    -- Order 5 items (Cancelled - Marmitas da Vo)
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd08',
        'cccccccc-cccc-cccc-cccc-cccccccccc05',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb002',
        'Marmita Vegana',
        'Legumes grelhados, arroz integral, grao de bico',
        '🥗',
        1,
        2000,
        2000
    ),
    
    -- Order 6 items (Older completed - Fit & Fresh)
    (
        'dddddddd-dddd-dddd-dddd-dddddddddd09',
        'cccccccc-cccc-cccc-cccc-cccccccccc06',
        'bbbbbbbb-bbbb-bbbb-bbbb-bbbbbbbbb004',
        'Bowl Fitness',
        'Quinoa, legumes, peito de peru, molho',
        '🥗',
        1,
        2200,
        2200
    )
ON CONFLICT (id) DO NOTHING;

-- Verify data integrity
DO $$
DECLARE
    order_count INTEGER;
    item_count INTEGER;
BEGIN
    SELECT COUNT(*) INTO order_count FROM orders;
    SELECT COUNT(*) INTO item_count FROM order_items;
    
    RAISE NOTICE 'Seed data loaded successfully:';
    RAISE NOTICE '  - Orders: %', order_count;
    RAISE NOTICE '  - Order Items: %', item_count;
END $$;
