import 'package:flutter/material.dart';
import 'api_controller.dart'; // para usar api, Product, CardSummary, etc.

class PointOfSalePage extends StatefulWidget {
  const PointOfSalePage({super.key});

  @override
  State<PointOfSalePage> createState() => _PointOfSalePageState();
}

class _PointOfSalePageState extends State<PointOfSalePage> {
  bool _isLoading = false;
  String _statusText = 'Waiting for NFC connection';
  String? _cardId;
  bool _started = false;

  @override
  void didChangeDependencies() {
    super.didChangeDependencies();
    if (!_started) {
      _started = true;
      _waitForCard();
    }
  }

  Future<void> _waitForCard() async {
    setState(() {
      _isLoading = true;
      _statusText = 'Waiting for NFC connection';
      _cardId = null;
    });

    final String? cardId = await api.waitForCard();

    if (!mounted) return;

    if (cardId == null) {
      setState(() {
        _isLoading = false;
        _statusText = 'Waiting for NFC connection';
      });
      await Future.delayed(const Duration(seconds: 1));
      if (mounted) _waitForCard();
      return;
    }

    setState(() {
      _isLoading = false;
      _cardId = cardId;
      _statusText = 'Cartão lido: $cardId';
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Point of Sale'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: _cardId == null
            ? Padding(
                padding: const EdgeInsets.all(24.0),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Icon(Icons.nfc, size: 72, color: Colors.green[700]),
                    const SizedBox(height: 16),
                    Text(
                      _statusText,
                      textAlign: TextAlign.center,
                      style: const TextStyle(fontSize: 18, color: Colors.black54),
                    ),
                    const SizedBox(height: 24),
                    if (_isLoading) const CircularProgressIndicator(),
                  ],
                ),
              )
            : Padding(
                padding: const EdgeInsets.symmetric(horizontal: 24.0),
                child: Column(
                  mainAxisAlignment: MainAxisAlignment.center,
                  children: [
                    Text(
                      'Cartão: $_cardId',
                      style: const TextStyle(fontSize: 18, fontWeight: FontWeight.bold),
                    ),
                    const SizedBox(height: 32),
                    ElevatedButton(
                      onPressed: () async {
                        await Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (_) => const AddConsumptionPage(),
                          ),
                        );
                        // Volta ao menu POS após add consumption
                      },
                      style: ElevatedButton.styleFrom(
                        minimumSize: const Size.fromHeight(50),
                        backgroundColor: Colors.green,
                      ),
                      child: const Text('Add consumption'),
                    ),
                    const SizedBox(height: 16),
                    ElevatedButton(
                      onPressed: () {
                        Navigator.push(
                          context,
                          MaterialPageRoute(
                            builder: (_) => const ViewConsumptionPage(),
                          ),
                        );
                      },
                      style: ElevatedButton.styleFrom(
                        minimumSize: const Size.fromHeight(50),
                        backgroundColor: Colors.green,
                      ),
                      child: const Text('View consumption'),
                    ),
                    const SizedBox(height: 16),
                    ElevatedButton(
                      onPressed: _waitForCard,
                      style: ElevatedButton.styleFrom(
                        minimumSize: const Size.fromHeight(50),
                        backgroundColor: Colors.orange,
                      ),
                      child: const Text('New card'),
                    ),
                  ],
                ),
              ),
      ),
    );
  }
}

// ======================================================================
// ADD CONSUMPTION
// ======================================================================

class AddConsumptionPage extends StatefulWidget {
  const AddConsumptionPage({super.key});

  @override
  State<AddConsumptionPage> createState() => _AddConsumptionPageState();
}

class _AddConsumptionPageState extends State<AddConsumptionPage> {
  bool _isLoading = false;
  String? _error;
  List<_ProductQuantity> _items = [];

  @override
  void initState() {
    super.initState();
    _loadProducts();
  }

  Future<void> _loadProducts() async {
    setState(() {
      _isLoading = true;
      _error = null;
      _items = [];
    });

    final products = await api.getProducts(); // pedido à API

    if (!mounted) return;

    if (products.isEmpty) {
      setState(() {
        _isLoading = false;
        _error = 'Não foi possível obter a lista de produtos.';
      });
      return;
    }

    setState(() {
      _isLoading = false;
      _items = products.map((p) => _ProductQuantity(product: p)).toList();
    });
  }

  void _increment(_ProductQuantity pq) {
    setState(() {
      pq.quantity++;
    });
  }

  void _decrement(_ProductQuantity pq) {
    if (pq.quantity == 0) return;
    setState(() {
      pq.quantity--;
    });
  }

  double get _total {
    double sum = 0;
    for (final item in _items) {
      sum += item.product.price * item.quantity;
    }
    return sum;
  }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/*   Future<void> _confirmAdd(BuildContext context) async {
    // Produtos com quantidade > 0
    final selected = _items.where((e) => e.quantity > 0).toList();

    if (selected.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Nenhum produto selecionado.')),
      );
      return;
    }

    final resultDialog = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        title: const Text('Confirm'),
        content: const Text('Add product(s) to consumption?'),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('Cancel'),
          ),
          TextButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('OK'),
          ),
        ],
      ),
    );

    if (resultDialog != true) return;

    // ---- LOADING ----
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => const Center(child: CircularProgressIndicator()),
    );

    bool allOk = true;

    // Envia productId + quantity para a API
    for (final item in selected) {
      final ok = await api.addConsumption(
        item.product.id, // productId
        item.quantity, // quantity
      );

      if (!ok) {
        allOk = false;
        break;
      }
    }

    Navigator.of(context).pop(); // fecha o loading

    if (!mounted) return;

    if (!allOk) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Erro ao registar consumo.')),
      );
      return;
    }

    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Added successfully.')),
    );

    // volta para o PointOfSalePage
    Navigator.pop(context);
  } */
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Future<void> _confirmAdd(BuildContext context) async {
  final selected = _items.where((e) => e.quantity > 0).toList();

  if (selected.isEmpty) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Nenhum produto selecionado.')),
    );
    return;
  }

  final resultDialog = await showDialog<bool>(
    context: context,
    builder: (context) => AlertDialog(
      title: const Text('Confirm'),
      content: const Text('Add product(s) to consumption?'),
      actions: [
        TextButton(
          onPressed: () => Navigator.of(context).pop(false),
          child: const Text('Cancel'),
        ),
        TextButton(
          onPressed: () => Navigator.of(context).pop(true),
          child: const Text('OK'),
        ),
      ],
    ),
  );

  if (resultDialog != true) return;

  showDialog(
    context: context,
    barrierDismissible: false,
    builder: (_) => const Center(child: CircularProgressIndicator()),
  );

  bool allOk = true;
  String allDebugInfo = '';

  for (final item in selected) {
    final result = await api.addConsumptionWithDebug(
      item.product.id,
      item.quantity,
    );

    allDebugInfo += result['debug'] as String;
    allDebugInfo += '\n-------------------\n';

    if (!(result['success'] as bool)) {
      allOk = false;
      break;
    }
  }

  Navigator.of(context).pop(); // fecha loading

  // MOSTRAR DEBUG
  if (!mounted) return;
  
  showDialog(
    context: context,
    builder: (ctx) => AlertDialog(
      title: const Text('Debug Info'),
      content: SingleChildScrollView(
        child: SelectableText(allDebugInfo),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(ctx),
          child: const Text('OK'),
        ),
      ],
    ),
  );

  if (!allOk) {
    ScaffoldMessenger.of(context).showSnackBar(
      const SnackBar(content: Text('Erro ao registar consumo.')),
    );
    return;
  }

  ScaffoldMessenger.of(context).showSnackBar(
    const SnackBar(content: Text('Added successfully.')),
  );

  Navigator.pop(context);
}
  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Add consumption'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: _isLoading
              ? const CircularProgressIndicator()
              : _error != null
                  ? Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Text(
                          _error!,
                          textAlign: TextAlign.center,
                          style: const TextStyle(color: Colors.red),
                        ),
                        const SizedBox(height: 16),
                        ElevatedButton(
                          onPressed: _loadProducts,
                          child: const Text('Retry'),
                        ),
                      ],
                    )
                  : Column(
                      children: [
                        Expanded(
                          child: ListView.builder(
                            itemCount: _items.length,
                            itemBuilder: (context, index) {
                              final item = _items[index];
                              return Card(
                                child: Padding(
                                  padding: const EdgeInsets.symmetric(
                                    vertical: 12.0,
                                    horizontal: 8.0,
                                  ),
                                  child: Row(
                                    children: [
                                      Expanded(
                                        flex: 3,
                                        child: Text(item.product.name),
                                      ),
                                      Expanded(
                                        flex: 2,
                                        child: Text(
                                          '€ ${item.product.price.toStringAsFixed(2)}',
                                        ),
                                      ),
                                      Row(
                                        children: [
                                          IconButton(
                                            onPressed: () =>
                                                _decrement(item),
                                            icon: const Icon(Icons.remove),
                                            color: Colors.green,
                                          ),
                                          Text('${item.quantity}'),
                                          IconButton(
                                            onPressed: () =>
                                                _increment(item),
                                            icon: const Icon(Icons.add),
                                            color: Colors.green,
                                          ),
                                        ],
                                      ),
                                    ],
                                  ),
                                ),
                              );
                            },
                          ),
                        ),
                        const SizedBox(height: 16),
                        Text(
                          'Total: € ${_total.toStringAsFixed(2)}',
                          style: const TextStyle(
                            fontSize: 18,
                            fontWeight: FontWeight.bold,
                          ),
                        ),
                        const SizedBox(height: 16),
                        ElevatedButton(
                          onPressed: () => _confirmAdd(context),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.green,
                            padding: const EdgeInsets.symmetric(
                              horizontal: 36,
                              vertical: 12,
                            ),
                          ),
                          child: const Text('Add'),
                        ),
                      ],
                    ),
        ),
      ),
    );
  }
}

class _ProductQuantity {
  final Product product;
  int quantity = 0;

  _ProductQuantity({required this.product});
}

// ======================================================================
// VIEW CONSUMPTION  (usa getConsumptionSummary + tabela)
// ======================================================================

class ViewConsumptionPage extends StatefulWidget {
  const ViewConsumptionPage({super.key});

  @override
  State<ViewConsumptionPage> createState() => _ViewConsumptionPageState();
}

class _ViewConsumptionPageState extends State<ViewConsumptionPage> {
  bool _isLoading = true;
  String? _error;
  CardSummary? _summary;

  @override
  void initState() {
    super.initState();
    _loadSummary();
  }

  Future<void> _loadSummary() async {
    setState(() {
      _isLoading = true;
      _error = null;
      _summary = null;
    });

    final s = await api.getConsumptionSummary();

    if (!mounted) return;

    if (s == null) {
      setState(() {
        _isLoading = false;
        _error =
            'Não foi possível obter o resumo dos consumos para o cartão atual.';
      });
      return;
    }

    setState(() {
      _isLoading = false;
      _summary = s;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('View consumption'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: _isLoading
              ? const CircularProgressIndicator()
              : _error != null
                  ? Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Text(
                          _error!,
                          textAlign: TextAlign.center,
                          style: const TextStyle(color: Colors.red),
                        ),
                        const SizedBox(height: 16),
                        ElevatedButton(
                          onPressed: _loadSummary,
                          child: const Text('Retry'),
                        ),
                        const SizedBox(height: 16),
                        ElevatedButton(
                          onPressed: () => Navigator.pop(context),
                          style: ElevatedButton.styleFrom(
                            backgroundColor: Colors.green,
                            padding: const EdgeInsets.symmetric(
                              horizontal: 36,
                              vertical: 12,
                            ),
                          ),
                          child: const Text('OK'),
                        ),
                      ],
                    )
                  : _buildSummaryContent(context),
        ),
      ),
    );
  }

  Widget _buildSummaryContent(BuildContext context) {
    final s = _summary!;
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Text(
          'Card ID: ${s.cardId}',
          style: const TextStyle(fontSize: 16),
        ),
        const SizedBox(height: 4),
        if (api.currentUserRole == "OWNER")
          Text('Telefone: ${s.phone}',
          style: const TextStyle(fontSize: 16),
        ),
        const SizedBox(height: 8),
        Text(
          'Total a pagar: € ${s.total.toStringAsFixed(2)}',
          style: const TextStyle(
            fontSize: 18,
            fontWeight: FontWeight.bold,
            color: Colors.green,
          ),
        ),
        const SizedBox(height: 16),
        Expanded(
          child: SingleChildScrollView(
            scrollDirection: Axis.vertical,
            child: SingleChildScrollView(
              scrollDirection: Axis.horizontal,
              child: DataTable(
                columns: const [
                  DataColumn(label: Text('Produto')),
                  DataColumn(label: Text('Qtd')),
                  DataColumn(label: Text('Preço')),
                  DataColumn(label: Text('Subtotal')),
                ],
                rows: s.lines
                    .map(
                      (line) => DataRow(
                        cells: [
                          DataCell(Text(line.productName)),
                          DataCell(Text('${line.quantity}')),
                          DataCell(Text(
                              '€ ${line.unitPrice.toStringAsFixed(2)}')),
                          DataCell(
                              Text('€ ${line.total.toStringAsFixed(2)}')),
                        ],
                      ),
                    )
                    .toList(),
              ),
            ),
          ),
        ),
        const SizedBox(height: 16),
        Align(
          alignment: Alignment.center,
          child: ElevatedButton(
            onPressed: () => Navigator.pop(context),
            style: ElevatedButton.styleFrom(
              backgroundColor: Colors.green,
              padding: const EdgeInsets.symmetric(
                horizontal: 36,
                vertical: 12,
              ),
            ),
            child: const Text('OK'),
          ),
        ),
      ],
    );
  }
}
