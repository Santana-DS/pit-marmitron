import 'product.dart';

/// Restaurant model
class Restaurant {
  final String id;
  final String name;
  final String emoji;
  final String bgColor;
  final double rating;
  final int etaMinutes;
  final List<Product> products;

  const Restaurant({
    required this.id,
    required this.name,
    required this.emoji,
    required this.bgColor,
    required this.rating,
    required this.etaMinutes,
    required this.products,
  });

  factory Restaurant.fromJson(Map<String, dynamic> json) {
    final productsJson = (json['products'] as List<dynamic>? ?? const [])
        .cast<Map<String, dynamic>>();

    return Restaurant(
      id: json['id'] as String,
      name: json['name'] as String,
      emoji: json['emoji'] as String? ?? '🍽️',
      bgColor: json['bg_color'] as String? ?? 'F5F5F5',
      rating: (json['rating'] as num?)?.toDouble() ?? 0.0,
      etaMinutes: json['eta_minutes'] as int? ?? 0,
      products: productsJson.map((p) => Product.fromJson(p)).toList(),
    );
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'name': name,
        'emoji': emoji,
        'bg_color': bgColor,
        'rating': rating,
        'eta_minutes': etaMinutes,
        'products': products.map((p) => p.toJson()).toList(),
      };

  Restaurant copyWith({
    String? id,
    String? name,
    String? emoji,
    String? bgColor,
    double? rating,
    int? etaMinutes,
    List<Product>? products,
  }) {
    return Restaurant(
      id: id ?? this.id,
      name: name ?? this.name,
      emoji: emoji ?? this.emoji,
      bgColor: bgColor ?? this.bgColor,
      rating: rating ?? this.rating,
      etaMinutes: etaMinutes ?? this.etaMinutes,
      products: products ?? this.products,
    );
  }
}
