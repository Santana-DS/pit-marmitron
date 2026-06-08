/// Product model
class Product {
  final String id;
  final String restaurantId;
  final String name;
  final String description;
  final String emoji;
  final double price;
  final bool isAvailable;
  final int sortOrder;
  int quantity;

  Product({
    required this.id,
    required this.restaurantId,
    required this.name,
    required this.description,
    required this.emoji,
    required this.price,
    required this.isAvailable,
    required this.sortOrder,
    this.quantity = 0,
  });

  int get priceCents => (price * 100).round();

  factory Product.fromJson(Map<String, dynamic> json) {
    return Product(
      id: json['id'] as String,
      restaurantId: json['restaurant_id'] as String? ?? '',
      name: json['name'] as String,
      description: json['description'] as String? ?? '',
      emoji: json['emoji'] as String? ?? '🍽️',
      price: ((json['price_cents'] as num?)?.toDouble() ?? 0) / 100,
      isAvailable: json['is_available'] as bool? ?? true,
      sortOrder: json['sort_order'] as int? ?? 0,
      quantity: json['quantity'] as int? ?? 0,
    );
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'restaurant_id': restaurantId,
        'name': name,
        'description': description,
        'emoji': emoji,
        'price_cents': priceCents,
        'is_available': isAvailable,
        'sort_order': sortOrder,
        'quantity': quantity,
      };

  Product copyWith({
    String? id,
    String? restaurantId,
    String? name,
    String? description,
    String? emoji,
    double? price,
    bool? isAvailable,
    int? sortOrder,
    int? quantity,
  }) {
    return Product(
      id: id ?? this.id,
      restaurantId: restaurantId ?? this.restaurantId,
      name: name ?? this.name,
      description: description ?? this.description,
      emoji: emoji ?? this.emoji,
      price: price ?? this.price,
      isAvailable: isAvailable ?? this.isAvailable,
      sortOrder: sortOrder ?? this.sortOrder,
      quantity: quantity ?? this.quantity,
    );
  }
}
