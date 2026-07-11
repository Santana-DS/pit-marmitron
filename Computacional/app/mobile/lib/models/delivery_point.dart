class DeliveryPoint {
  final String pointKey;
  final String label;
  final String displayAddress;
  final double latitude;
  final double longitude;

  const DeliveryPoint({
    required this.pointKey,
    required this.label,
    required this.displayAddress,
    required this.latitude,
    required this.longitude,
  });

  factory DeliveryPoint.fromJson(Map<String, dynamic> json) => DeliveryPoint(
        pointKey: json['point_key'] as String,
        label: json['label'] as String,
        displayAddress: json['display_address'] as String,
        latitude: (json['latitude'] as num).toDouble(),
        longitude: (json['longitude'] as num).toDouble(),
      );
}
