import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import '../models/product.dart';
import 'api_service.dart';

class ProductService {
  const ProductService();

  /// Get product by ID
  Future<Product> getProductById(String productId) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/products/$productId');

    final response = await http.get(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode == 404) {
      throw Exception('Product not found');
    }

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to get product: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return Product.fromJson(data);
  }

  /// List products by restaurant
  Future<List<Product>> listProductsByRestaurant(
    String restaurantId, {
    bool? available,
  }) async {
    final queryParams = <String, String>{
      if (available != null) 'available': available.toString(),
    };

    final uri = Uri.parse(
            '${ApiService.baseUrl}/api/restaurants/$restaurantId/products')
        .replace(queryParameters: queryParams.isEmpty ? null : queryParams);

    final response = await http.get(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to list products: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    final productsJson = (data['products'] as List<dynamic>? ?? const [])
        .cast<Map<String, dynamic>>();

    return productsJson.map((p) => Product.fromJson(p)).toList();
  }

  /// Create a new product
  Future<Product> createProduct({
    required String restaurantId,
    required String name,
    required String description,
    required int priceCents,
    String? emoji,
    bool available = true,
  }) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/products');

    final body = jsonEncode({
      'restaurant_id': restaurantId,
      'name': name,
      'description': description,
      'price_cents': priceCents,
      if (emoji != null) 'emoji': emoji,
      'available': available,
    });

    final response = await http
        .post(
          uri,
          headers: {'Content-Type': 'application/json'},
          body: body,
        )
        .timeout(ApiService.apiTimeout);

    if (response.statusCode != 201) {
      final error = _extractError(response.body);
      throw Exception('Failed to create product: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return Product.fromJson(data);
  }

  /// Update product information
  Future<Product> updateProduct({
    required String productId,
    String? name,
    String? description,
    int? priceCents,
    String? emoji,
    bool? available,
  }) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/products/$productId');

    final body = jsonEncode({
      if (name != null) 'name': name,
      if (description != null) 'description': description,
      if (priceCents != null) 'price_cents': priceCents,
      if (emoji != null) 'emoji': emoji,
      if (available != null) 'available': available,
    });

    final response = await http
        .patch(
          uri,
          headers: {'Content-Type': 'application/json'},
          body: body,
        )
        .timeout(ApiService.apiTimeout);

    if (response.statusCode == 404) {
      throw Exception('Product not found');
    }

    if (response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to update product: $error');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return Product.fromJson(data);
  }

  /// Delete a product
  Future<void> deleteProduct(String productId) async {
    final uri = Uri.parse('${ApiService.baseUrl}/api/products/$productId');

    final response = await http.delete(uri).timeout(ApiService.apiTimeout);

    if (response.statusCode == 404) {
      throw Exception('Product not found');
    }

    if (response.statusCode != 204 && response.statusCode != 200) {
      final error = _extractError(response.body);
      throw Exception('Failed to delete product: $error');
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
}
