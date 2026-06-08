import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import '../models/order.dart';
import '../models/order_item.dart';
import 'api_service.dart';

class OrderService {
  const OrderService();

  /// Create a new order
  ///
  /// [clientId] is optional - if not provided, backend will use mock user
  /// TODO: Remove optional clientId when user authentication is implemented
  Future<OrderWithItems> createOrder({
    String? clientId,
    required String restaurantId,
    required String deliveryAddress,
    required List<OrderItemRequest> items,
  }) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/orders');

    final requestBody = <String, dynamic>{
      'restaurant_id': restaurantId,
      'delivery_address': deliveryAddress,
      'items': items.map((item) => item.toJson()).toList(),
    };
    
    // Only include client_id if provided (let backend inject mock if empty)
    if (clientId != null && clientId.isNotEmpty) {
      requestBody['client_id'] = clientId;
    }

    final body = jsonEncode(requestBody);

    final response = await http
        .post(
          uri,
          headers: {'Content-Type': 'application/json'},
          body: body,
        )
        .timeout(ApiService.apiTimeout);

    if (response.statusCode != 201) {
      final error = _extractError(response.body);
      throw Exception('Failed to create order: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    final orderData = data['order'] as Map<String, dynamic>;
    return OrderWithItems.fromJson(orderData);
  }

  /// Get order by ID
  Future<OrderWithItems> getOrderById(String orderId) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/orders/$orderId');

    final response = await http.get(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode == 404) {
      throw Exception('Order not found');
    }

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to get order: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return OrderWithItems.fromJson(data);
  }

  /// List orders for a client
  Future<OrderListResponse> listOrdersByClient(
    String clientId, {
    int limit = 20,
    int offset = 0,
  }) async {
    final uri = Uri.parse(
            '${ApiService.baseUrl}/api/clients/$clientId/orders')
        .replace(queryParameters: {
      'limit': limit.toString(),
      'offset': offset.toString(),
    });

    final response = await http.get(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to list orders: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return _orderListResponseFromJson(data);
  }

  /// List orders for a restaurant
  Future<OrderListResponse> listOrdersByRestaurant(
    String restaurantId, {
    int limit = 20,
    int offset = 0,
  }) async {
    final uri = Uri.parse(
            '${ApiService.baseUrl}/api/restaurants/$restaurantId/orders')
        .replace(queryParameters: {
      'limit': limit.toString(),
      'offset': offset.toString(),
    });

    final response = await http.get(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to list restaurant orders: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return _orderListResponseFromJson(data);
  }

  /// Update order status
  Future<void> updateOrderStatus(String orderId, String status) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/orders/$orderId/status');

    final body = jsonEncode({'status': status});

    final response = await http
        .patch(
          uri,
          headers: {'Content-Type': 'application/json'},
          body: body,
        )
        .timeout(ApiService.apiTimeout);

    if (response.statusCode == 404) {
      throw Exception('Order not found');
    }

    if (response.statusCode == 400) {
      final error = _extractError(response.body);
      throw Exception('Invalid status: $error');
    }

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to update order status: $error');
    }
  }

  // ─── Helper methods ──────────────────────────────────────────────────────────

  String _extractError(String body) {
    try {
      final data = jsonDecode(body) as Map<String, dynamic>;
      return data['error'] as String? ?? 'Unknown error';
    } catch (_) {
      return 'Unknown error';
    }
  }

  OrderListResponse _orderListResponseFromJson(Map<String, dynamic> json) {
    final ordersJson =
        (json['orders'] as List<dynamic>? ?? const []).cast<Map<String, dynamic>>();

    return OrderListResponse(
      orders: ordersJson.map((o) => OrderWithItems.fromJson(o)).toList(),
      total: json['total'] as int,
      limit: json['limit'] as int,
      offset: json['offset'] as int,
    );
  }
}

// ─── Response Models ─────────────────────────────────────────────────────────

class OrderListResponse {
  final List<OrderWithItems> orders;
  final int total;
  final int limit;
  final int offset;

  const OrderListResponse({
    required this.orders,
    required this.total,
    required this.limit,
    required this.offset,
  });
}
