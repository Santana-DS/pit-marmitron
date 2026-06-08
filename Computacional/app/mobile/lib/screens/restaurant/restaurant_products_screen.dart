// ignore_for_file: prefer_const_constructors

import 'package:flutter/material.dart';
import 'package:google_fonts/google_fonts.dart';
import '../../theme/app_theme.dart';
import '../../models/models.dart';
import '../../services/product_service.dart';
import '../../services/restaurant_service.dart';
import '../../state/user_state.dart';
import '../../widgets/widgets.dart';

class RestaurantProductsScreen extends StatefulWidget {
  final bool standalone;

  const RestaurantProductsScreen({super.key, this.standalone = true});

  @override
  State<RestaurantProductsScreen> createState() =>
      _RestaurantProductsScreenState();
}

class _RestaurantProductsScreenState
    extends State<RestaurantProductsScreen> {
  final ProductService _productService = const ProductService();
  List<_ProductEntry> _products = [];
  bool _isLoading = true;
  String? _error;

  @override
  void initState() {
    super.initState();
    _loadProducts();
  }

  Future<void> _loadProducts() async {
    setState(() {
      _isLoading = true;
      _error = null;
    });

    try {
      final restaurantId =
          userStateNotifier.value.restaurantProfile?.restaurantId ??
              kMockRestaurantProfile.restaurantId;
      
      // Fetch restaurant details with products from the seeded mock restaurant
      final restaurant =
          await RestaurantService().getRestaurantById(restaurantId);
      
      setState(() {
        _products = restaurant.products
            .map((product) => _ProductEntry(
                  product: product,
                  sales: 0,
                ))
            .toList();
        _isLoading = false;
      });
    } catch (e) {
      setState(() {
        _error = 'Erro ao carregar produtos: $e';
        _isLoading = false;
      });
    }
  }

  Future<void> _toggleAvailability(int index) async {
    final entry = _products[index];
    final nextAvailability = !entry.product.isAvailable;

    try {
      final updated = await _productService.updateProduct(
        productId: entry.product.id,
        available: nextAvailability,
      );

      if (!mounted) return;
      setState(() {
        _products[index] = entry.copyWith(product: updated);
      });
      showAppSnack(
        context,
        nextAvailability ? 'Produto disponibilizado.' : 'Produto marcado como esgotado.',
      );
    } catch (e) {
      if (!mounted) return;
      showAppSnack(context, 'Erro ao atualizar disponibilidade: $e');
    }
  }

  Future<void> _createProduct(_ProductFormData formData) async {
    final restaurantId =
        userStateNotifier.value.restaurantProfile?.restaurantId ??
            kMockRestaurantProfile.restaurantId;

    try {
      final created = await _productService.createProduct(
        restaurantId: restaurantId,
        name: formData.name,
        description: formData.description,
        priceCents: formData.priceCents,
        emoji: formData.emoji,
        available: true,
      );

      if (!mounted) return;
      setState(() {
        _products = [
          _ProductEntry(product: created, sales: 0),
          ..._products,
        ];
      });
      showAppSnack(context, 'Produto adicionado com sucesso.');
    } catch (e) {
      if (!mounted) return;
      showAppSnack(context, 'Erro ao adicionar produto: $e');
    }
  }

  Future<void> _updateProduct(_ProductEntry entry, _ProductFormData formData) async {
    try {
      final updated = await _productService.updateProduct(
        productId: entry.product.id,
        name: formData.name,
        description: formData.description,
        priceCents: formData.priceCents,
        emoji: formData.emoji,
      );

      if (!mounted) return;
      setState(() {
        final index =
            _products.indexWhere((item) => item.product.id == entry.product.id);
        if (index != -1) {
          _products[index] = entry.copyWith(product: updated);
        }
      });
      showAppSnack(context, 'Produto atualizado com sucesso.');
    } catch (e) {
      if (!mounted) return;
      showAppSnack(context, 'Erro ao atualizar produto: $e');
    }
  }

  @override
  Widget build(BuildContext context) {
    if (_isLoading) {
      return Scaffold(
        backgroundColor: AC.surface(context),
        appBar: widget.standalone
            ? AppBar(
                title: const Text('Gerenciar Produtos'),
                backgroundColor: AC.surface(context),
              )
            : null,
        body: Center(child: CircularProgressIndicator()),
      );
    }

    if (_error != null) {
      return Scaffold(
        backgroundColor: AC.surface(context),
        appBar: widget.standalone
            ? AppBar(
                title: const Text('Gerenciar Produtos'),
                backgroundColor: AC.surface(context),
              )
            : null,
        body: Center(
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              Text(
                _error!,
                style: GoogleFonts.dmSans(
                  fontSize: 14,
                  color: AC.muted(context),
                ),
                textAlign: TextAlign.center,
              ),
              const SizedBox(height: 16),
              ElevatedButton(
                onPressed: _loadProducts,
                child: const Text('Tentar novamente'),
              ),
            ],
          ),
        ),
      );
    }

    final content = CustomScrollView(
      slivers: [
        if (widget.standalone)
          SliverAppBar(
            floating: true,
            title: const Text('Gerenciar Produtos'),
            actions: [
              IconButton(
                icon: const Icon(Icons.add_rounded, color: AppColors.accent),
                onPressed: () => _showAddProductSheet(context),
              ),
            ],
          )
        else
          SliverToBoxAdapter(
            child: Padding(
              padding: const EdgeInsets.fromLTRB(20, 20, 20, 0),
              child: Row(
                mainAxisAlignment: MainAxisAlignment.spaceBetween,
                children: [
                  Text('Produtos',
                      style: Theme.of(context).textTheme.displaySmall),
                  GestureDetector(
                    behavior: HitTestBehavior.opaque,
                    onTap: () => _showAddProductSheet(context),
                    child: Container(
                      padding: const EdgeInsets.symmetric(
                          horizontal: 14, vertical: 8),
                      decoration: BoxDecoration(
                        color: AppColors.accent,
                        borderRadius: BorderRadius.circular(10),
                      ),
                      child: Row(
                        children: [
                          const Icon(Icons.add_rounded,
                              color: Colors.white, size: 16),
                          const SizedBox(width: 4),
                          Text('Adicionar',
                              style: GoogleFonts.dmSans(
                                  fontSize: 13,
                                  color: Colors.white,
                                  fontWeight: FontWeight.w500)),
                        ],
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ),

        SliverPadding(
          padding: const EdgeInsets.all(20),
          sliver: SliverList(
            delegate: SliverChildBuilderDelegate(
              (ctx, i) => Padding(
                padding: const EdgeInsets.only(bottom: 12),
                child: _ProductTile(
                  entry: _products[i],
                  onToggle: () => _toggleAvailability(i),
                  onEdit: () => _showEditSheet(context, _products[i]),
                ),
              ),
              childCount: _products.length,
            ),
          ),
        ),
      ],
    );

    return widget.standalone
        ? Scaffold(backgroundColor: AppColors.surface, body: content)
        : content;
  }

  void _showAddProductSheet(BuildContext context) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: AppColors.surface,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (_) => _ProductFormSheet(
        title: 'Novo produto',
        onSubmit: _createProduct,
      ),
    );
  }

  void _showEditSheet(BuildContext context, _ProductEntry entry) {
    showModalBottomSheet(
      context: context,
      isScrollControlled: true,
      backgroundColor: AppColors.surface,
      shape: const RoundedRectangleBorder(
        borderRadius: BorderRadius.vertical(top: Radius.circular(20)),
      ),
      builder: (_) => _ProductFormSheet(
        title: 'Editar produto',
        entry: entry,
        onSubmit: (formData) => _updateProduct(entry, formData),
      ),
    );
  }
}

class _ProductEntry {
  final Product product;
  final int sales;

  const _ProductEntry({
    required this.product,
    required this.sales,
  });

  _ProductEntry copyWith({
    Product? product,
    int? sales,
  }) {
    return _ProductEntry(
      product: product ?? this.product,
      sales: sales ?? this.sales,
    );
  }
}

class _ProductFormData {
  final String name;
  final String description;
  final int priceCents;
  final String emoji;

  const _ProductFormData({
    required this.name,
    required this.description,
    required this.priceCents,
    required this.emoji,
  });
}

class _ProductTile extends StatelessWidget {
  final _ProductEntry entry;
  final VoidCallback onToggle;
  final VoidCallback onEdit;

  const _ProductTile({
    required this.entry,
    required this.onToggle,
    required this.onEdit,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      decoration: BoxDecoration(
        color: AppColors.card,
        borderRadius: BorderRadius.circular(16),
        border: Border.all(color: AppColors.primary.withValues(alpha: 0.08)),
      ),
      padding: const EdgeInsets.all(14),
      child: Row(
        children: [
          Container(
            width: 52,
            height: 52,
            decoration: BoxDecoration(
              color: AppColors.surface,
              borderRadius: BorderRadius.circular(12),
            ),
            child: Center(
              child: Text(entry.product.emoji,
                  style: const TextStyle(fontSize: 28)),
            ),
          ),
          const SizedBox(width: 12),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  entry.product.name,
                  style: GoogleFonts.dmSans(
                    fontSize: 14,
                    fontWeight: FontWeight.w500,
                    color: AppColors.primary,
                  ),
                ),
                const SizedBox(height: 2),
                Text(
                  entry.product.description,
                  style: GoogleFonts.dmSans(
                      fontSize: 11, color: AppColors.muted),
                  maxLines: 1,
                  overflow: TextOverflow.ellipsis,
                ),
                const SizedBox(height: 6),
                Row(
                  children: [
                    StatusBadge(
                      label: entry.product.isAvailable ? 'Disponível' : 'Esgotado',
                      bg: entry.product.isAvailable
                          ? AppColors.statusDelivered
                          : AppColors.statusPending,
                      textColor: entry.product.isAvailable
                          ? AppColors.statusDeliveredText
                          : AppColors.statusPendingText,
                    ),
                    const SizedBox(width: 8),
                    Text(
                      '${entry.sales} vendidos',
                      style: GoogleFonts.dmSans(
                          fontSize: 11, color: AppColors.muted),
                    ),
                  ],
                ),
              ],
            ),
          ),
          Column(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Text(
                'R\$${entry.product.price.toStringAsFixed(2).replaceAll('.', ',')}',
                style: GoogleFonts.spaceGrotesk(
                  fontSize: 14,
                  fontWeight: FontWeight.w600,
                  color: AppColors.accent,
                ),
              ),
              const SizedBox(height: 8),
              Row(
                children: [
                  GestureDetector(
                    onTap: onEdit,
                    child: Container(
                      padding: const EdgeInsets.all(6),
                      decoration: BoxDecoration(
                        color: AppColors.primary.withValues(alpha: 0.06),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: const Icon(Icons.edit_outlined,
                          size: 16, color: AppColors.muted),
                    ),
                  ),
                  const SizedBox(width: 6),
                  GestureDetector(
                    onTap: onToggle,
                    child: Container(
                      padding: const EdgeInsets.all(6),
                      decoration: BoxDecoration(
                        color: entry.product.isAvailable
                            ? AppColors.teal.withValues(alpha: 0.1)
                            : Colors.red.withValues(alpha: 0.08),
                        borderRadius: BorderRadius.circular(8),
                      ),
                      child: Icon(
                        entry.product.isAvailable
                            ? Icons.toggle_on_rounded
                            : Icons.toggle_off_rounded,
                        size: 18,
                        color: entry.product.isAvailable
                            ? AppColors.teal
                            : Colors.red,
                      ),
                    ),
                  ),
                ],
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _ProductFormSheet extends StatefulWidget {
  final String title;
  final _ProductEntry? entry;
  final Future<void> Function(_ProductFormData formData) onSubmit;

  const _ProductFormSheet({
    required this.title,
    required this.onSubmit,
    this.entry,
  });

  @override
  State<_ProductFormSheet> createState() => _ProductFormSheetState();
}

class _ProductFormSheetState extends State<_ProductFormSheet> {
  late final TextEditingController _nameCtrl;
  late final TextEditingController _descriptionCtrl;
  late final TextEditingController _priceCtrl;
  late final TextEditingController _emojiCtrl;
  bool _saving = false;

  @override
  void initState() {
    super.initState();
    _nameCtrl = TextEditingController(text: widget.entry?.product.name ?? '');
    _descriptionCtrl =
        TextEditingController(text: widget.entry?.product.description ?? '');
    _priceCtrl = TextEditingController(
      text: widget.entry != null
          ? widget.entry!.product.price.toStringAsFixed(2).replaceAll('.', ',')
          : '',
    );
    _emojiCtrl =
        TextEditingController(text: widget.entry?.product.emoji ?? '🍱');
  }

  @override
  void dispose() {
    _nameCtrl.dispose();
    _descriptionCtrl.dispose();
    _priceCtrl.dispose();
    _emojiCtrl.dispose();
    super.dispose();
  }

  Future<void> _submit() async {
    final name = _nameCtrl.text.trim();
    final description = _descriptionCtrl.text.trim();
    final emoji = _emojiCtrl.text.trim().isEmpty ? '🍱' : _emojiCtrl.text.trim();
    final normalizedPrice = _priceCtrl.text.trim().replaceAll(',', '.');
    final price = double.tryParse(normalizedPrice);

    if (name.isEmpty) {
      showAppSnack(context, 'Informe o nome do produto.');
      return;
    }
    if (price == null || price < 0) {
      showAppSnack(context, 'Informe um preço válido.');
      return;
    }

    setState(() => _saving = true);
    try {
      await widget.onSubmit(
        _ProductFormData(
          name: name,
          description: description,
          priceCents: (price * 100).round(),
          emoji: emoji,
        ),
      );
      if (!mounted) return;
      Navigator.pop(context);
    } finally {
      if (mounted) {
        setState(() => _saving = false);
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: EdgeInsets.only(
        bottom: MediaQuery.of(context).viewInsets.bottom,
      ),
      child: SingleChildScrollView(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          mainAxisSize: MainAxisSize.min,
          children: [
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(widget.title,
                    style: Theme.of(context).textTheme.displaySmall),
                IconButton(
                  icon: const Icon(Icons.close_rounded),
                  onPressed: _saving ? null : () => Navigator.pop(context),
                ),
              ],
            ),
            const SizedBox(height: 20),
            const FormFieldLabel('Nome do produto'),
            TextField(
              controller: _nameCtrl,
              decoration:
                  const InputDecoration(hintText: 'Ex: Marmita Executiva'),
            ),
            const SizedBox(height: 14),
            const FormFieldLabel('Descrição'),
            TextField(
              controller: _descriptionCtrl,
              maxLines: 2,
              decoration: const InputDecoration(
                  hintText: 'Descreva os ingredientes...'),
            ),
            const SizedBox(height: 14),
            Row(
              children: [
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const FormFieldLabel('Preço (R\$)'),
                      TextField(
                        controller: _priceCtrl,
                        keyboardType: const TextInputType.numberWithOptions(
                          decimal: true,
                        ),
                        decoration:
                            const InputDecoration(hintText: '0,00'),
                      ),
                    ],
                  ),
                ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      const FormFieldLabel('Emoji'),
                      TextField(
                        controller: _emojiCtrl,
                        decoration:
                            const InputDecoration(hintText: '🍱'),
                      ),
                    ],
                  ),
                ),
              ],
            ),
            const SizedBox(height: 24),
            AppButton(
              label: widget.entry != null
                  ? 'Salvar alterações'
                  : 'Adicionar produto',
              onTap: _submit,
              loading: _saving,
            ),
            const SizedBox(height: 8),
          ],
        ),
      ),
    );
  }
}
