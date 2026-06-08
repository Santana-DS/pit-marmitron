/// Order item model representing a product in an order
class OrderItem {
  final String id;
  final String orderId;
  final String? productId;
  final String productName;
  final String productDescription;
  final String productEmoji;
  final int quantity;
  final int unitPriceCents;
  final int totalPriceCents;
  final DateTime? createdAt;

  const OrderItem({
    required this.id,
    required this.orderId,
    required this.productId,
    required this.productName,
    required this.productDescription,
    required this.productEmoji,
    required this.quantity,
    required this.unitPriceCents,
    required this.totalPriceCents,
    required this.createdAt,
  });

  double get unitPrice => unitPriceCents / 100.0;
  double get totalPrice => totalPriceCents / 100.0;
  double get subtotal => totalPrice;

  factory OrderItem.fromJson(Map<String, dynamic> json) {
    return OrderItem(
      id: json['id'] as String? ?? '',
      orderId: json['order_id'] as String? ?? '',
      productId: json['product_id'] as String?,
      productName: json['product_name'] as String? ?? 'Produto',
      productDescription: json['product_description'] as String? ?? '',
      productEmoji: json['product_emoji'] as String? ?? '🍽️',
      quantity: json['quantity'] as int? ?? 0,
      unitPriceCents: json['unit_price_cents'] as int? ?? 0,
      totalPriceCents: json['total_price_cents'] as int? ??
          json['subtotal_cents'] as int? ??
          0,
      createdAt: (json['created_at'] as String?) != null
          ? DateTime.parse(json['created_at'] as String)
          : null,
    );
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'order_id': orderId,
        'product_id': productId,
        'product_name': productName,
        'product_description': productDescription,
        'product_emoji': productEmoji,
        'quantity': quantity,
        'unit_price_cents': unitPriceCents,
        'total_price_cents': totalPriceCents,
        'created_at': createdAt?.toIso8601String(),
      };
}

/// Request model for creating an order item
class OrderItemRequest {
  final String productId;
  final int quantity;
  final int unitPriceCents;

  const OrderItemRequest({
    required this.productId,
    required this.quantity,
    required this.unitPriceCents,
  });

  Map<String, dynamic> toJson() => {
        'product_id': productId,
        'quantity': quantity,
        'unit_price_cents': unitPriceCents,
      };
}
