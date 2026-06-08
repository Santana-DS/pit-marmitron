import 'package:unbot/models/order_item.dart';

/// Order model representing a customer order
class Order {
  final String id;
  final String publicCode;
  final String clientId;
  final String restaurantId;
  final String restaurantName;
  final String deliveryAddress;
  final String status;
  final int subtotalCents;
  final int deliveryFeeCents;
  final int discountCents;
  final int totalCents;
  final bool robotDispatched;
  final String? gatewayMode;
  final bool mqttConnected;
  final DateTime placedAt;
  final DateTime? dispatchedAt;
  final DateTime? completedAt;
  final DateTime? cancelledAt;
  final String? cancelReason;
  final String? notes;
  final DateTime createdAt;
  final DateTime updatedAt;

  const Order({
    required this.id,
    required this.publicCode,
    required this.clientId,
    required this.restaurantId,
    required this.restaurantName,
    required this.deliveryAddress,
    required this.status,
    required this.subtotalCents,
    required this.deliveryFeeCents,
    required this.discountCents,
    required this.totalCents,
    required this.robotDispatched,
    required this.gatewayMode,
    required this.mqttConnected,
    required this.placedAt,
    required this.dispatchedAt,
    required this.completedAt,
    required this.cancelledAt,
    required this.cancelReason,
    required this.notes,
    required this.createdAt,
    required this.updatedAt,
  });

  double get subtotal => subtotalCents / 100.0;
  double get deliveryFee => deliveryFeeCents / 100.0;
  double get discount => discountCents / 100.0;
  double get total => totalCents / 100.0;

  OrderStatus get orderStatus {
    switch (status.toLowerCase()) {
      case 'pending':
        return OrderStatus.pending;
      case 'preparing':
        return OrderStatus.preparing;
      case 'dispatched':
      case 'on_the_way':
        return OrderStatus.onTheWay;
      case 'delivered':
        return OrderStatus.delivered;
      case 'cancelled':
        return OrderStatus.cancelled;
      default:
        return OrderStatus.pending;
    }
  }

  factory Order.fromJson(Map<String, dynamic> json) {
    DateTime? parseOptionalDate(String key) {
      final value = json[key] as String?;
      return value == null || value.isEmpty ? null : DateTime.parse(value);
    }

    return Order(
      id: json['id'] as String,
      publicCode: json['public_code'] as String? ?? '',
      clientId:
          json['client_user_id'] as String? ?? json['client_id'] as String? ?? '',
      restaurantId: json['restaurant_id'] as String,
      restaurantName: json['restaurant_name'] as String? ?? '',
      deliveryAddress: json['delivery_address'] as String,
      status: json['status'] as String,
      subtotalCents: json['subtotal_cents'] as int? ?? 0,
      deliveryFeeCents: json['delivery_fee_cents'] as int? ?? 0,
      discountCents: json['discount_cents'] as int? ?? 0,
      totalCents: json['total_cents'] as int? ?? 0,
      robotDispatched: json['robot_dispatched'] as bool? ?? false,
      gatewayMode: json['gateway_mode'] as String?,
      mqttConnected: json['mqtt_connected'] as bool? ?? false,
      placedAt: DateTime.parse(
        json['placed_at'] as String? ?? json['created_at'] as String,
      ),
      dispatchedAt: parseOptionalDate('dispatched_at'),
      completedAt: parseOptionalDate('completed_at'),
      cancelledAt: parseOptionalDate('cancelled_at'),
      cancelReason: json['cancel_reason'] as String?,
      notes: json['notes'] as String?,
      createdAt: DateTime.parse(json['created_at'] as String),
      updatedAt: DateTime.parse(json['updated_at'] as String),
    );
  }

  Map<String, dynamic> toJson() => {
        'id': id,
        'public_code': publicCode,
        'client_user_id': clientId,
        'restaurant_id': restaurantId,
        'restaurant_name': restaurantName,
        'delivery_address': deliveryAddress,
        'status': status,
        'subtotal_cents': subtotalCents,
        'delivery_fee_cents': deliveryFeeCents,
        'discount_cents': discountCents,
        'total_cents': totalCents,
        'robot_dispatched': robotDispatched,
        'gateway_mode': gatewayMode,
        'mqtt_connected': mqttConnected,
        'placed_at': placedAt.toIso8601String(),
        'dispatched_at': dispatchedAt?.toIso8601String(),
        'completed_at': completedAt?.toIso8601String(),
        'cancelled_at': cancelledAt?.toIso8601String(),
        'cancel_reason': cancelReason,
        'notes': notes,
        'created_at': createdAt.toIso8601String(),
        'updated_at': updatedAt.toIso8601String(),
      };
}

/// Order with items - complete order details
class OrderWithItems {
  final Order order;
  final List<OrderItem> items;

  const OrderWithItems({
    required this.order,
    required this.items,
  });

  factory OrderWithItems.fromJson(Map<String, dynamic> json) {
    final itemsJson = (json['items'] as List<dynamic>? ?? const [])
        .cast<Map<String, dynamic>>();

    return OrderWithItems(
      order: Order.fromJson(json),
      items: itemsJson.map((item) => OrderItem.fromJson(item)).toList(),
    );
  }

  Map<String, dynamic> toJson() => {
        ...order.toJson(),
        'items': items.map((item) => item.toJson()).toList(),
      };
}

/// Order status enum
enum OrderStatus { pending, preparing, onTheWay, delivered, cancelled }

extension OrderStatusExt on OrderStatus {
  String get label {
    switch (this) {
      case OrderStatus.pending:
        return 'Pendente';
      case OrderStatus.preparing:
        return 'Em preparo';
      case OrderStatus.onTheWay:
        return 'A caminho';
      case OrderStatus.delivered:
        return 'Entregue';
      case OrderStatus.cancelled:
        return 'Cancelado';
    }
  }

  String get value {
    switch (this) {
      case OrderStatus.pending:
        return 'pending';
      case OrderStatus.preparing:
        return 'preparing';
      case OrderStatus.onTheWay:
        return 'on_the_way';
      case OrderStatus.delivered:
        return 'delivered';
      case OrderStatus.cancelled:
        return 'cancelled';
    }
  }
}
