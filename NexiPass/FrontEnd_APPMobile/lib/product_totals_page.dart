import 'package:flutter/material.dart';
import 'api_controller.dart';

class ProductTotalsPage extends StatefulWidget {
  const ProductTotalsPage({super.key});

  @override
  State<ProductTotalsPage> createState() => _ProductTotalsPageState();
}

class _ProductTotalsPageState extends State<ProductTotalsPage> {
  bool _isLoading = true;
  List<ProductTotal> _totals = [];
  String? _error;

  @override
  void initState() {
    super.initState();
    _loadTotals();
  }

  Future<void> _loadTotals() async {
    setState(() {
      _isLoading = true;
      _error = null;
    });

    final totals = await api.getProductTotals();

    if (!mounted) return;

    if (totals.isEmpty) {
      setState(() {
        _isLoading = false;
        _error = 'Nenhum dado disponível';
      });
      return;
    }

    setState(() {
      _isLoading = false;
      _totals = totals;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Vending\'s Totals'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(16.0),
          child: _isLoading
              ? const CircularProgressIndicator()
              : _error != null
                  ? Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      children: [
                        Text(_error!, style: const TextStyle(color: Colors.red)),
                        const SizedBox(height: 16),
                        ElevatedButton(
                          onPressed: _loadTotals,
                          child: const Text('Retry'),
                        ),
                      ],
                    )
                  : SingleChildScrollView(
                      scrollDirection: Axis.vertical,
                      child: SingleChildScrollView(
                        scrollDirection: Axis.horizontal,
                        child: DataTable(
                          columns: const [
                            DataColumn(label: Text('Produto')),
                            DataColumn(label: Text('Funcionário')),
                            DataColumn(label: Text('Quantidade')),
                            DataColumn(label: Text('Receita (€)')),
                          ],
                          rows: _totals
                              .map(
                                (t) => DataRow(
                                  cells: [
                                    DataCell(Text(t.productName)),
                                    DataCell(Text(t.employeeName)),
                                    DataCell(Text('${t.totalQuantity}')),
                                    DataCell(Text('€ ${t.totalRevenue.toStringAsFixed(2)}')),
                                  ],
                                ),
                              )
                              .toList(),
                        ),
                      ),
                    ),
        ),
      ),
    );
  }
}