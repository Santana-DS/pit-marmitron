import 'dart:async';
import 'dart:convert';

import 'package:http/http.dart' as http;

import '../models/restaurant.dart';
import 'api_service.dart';

class RestaurantService {
  const RestaurantService();

  Future<List<Restaurant>> listRestaurants({String query = ''}) async {
    final trimmed = query.trim();
    final uri = Uri.parse('${ApiService.baseUrl}/api/restaurants')
        .replace(queryParameters: trimmed.isEmpty ? null : {'q': trimmed});

    final response = await http.get(uri).timeout(ApiService.apiTimeout);
    if (response.statusCode != 200) {
      throw Exception('Failed to load restaurants: ${response.statusCode}');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    final items = (data['restaurants'] as List<dynamic>? ?? const [])
        .cast<Map<String, dynamic>>();

    return items.map(_restaurantFromJson).toList();
  }

  Future<Restaurant> getRestaurantById(String restaurantId) async {
    final uri =
        Uri.parse('${ApiService.baseUrl}/api/restaurants/$restaurantId');

    final response = await http.get(uri).timeout(ApiService.apiTimeout);
    if (response.statusCode != 200) {
      throw Exception('Failed to load restaurant: ${response.statusCode}');
    }

    final data = jsonDecode(response.body) as Map<String, dynamic>;
    return _restaurantDetailsFromJson(data);
  }

  Restaurant _restaurantFromJson(Map<String, dynamic> json) {
    return Restaurant.fromJson(json).copyWith(products: const []);
  }

  Restaurant _restaurantDetailsFromJson(Map<String, dynamic> json) {
    return Restaurant.fromJson(json);
  }
}
