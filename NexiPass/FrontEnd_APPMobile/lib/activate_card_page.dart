import 'package:flutter/material.dart';
import 'package:flutter/services.dart';
import 'api_controller.dart';
import 'card_activated_page.dart';

class ActivateCardPage extends StatefulWidget {
  const ActivateCardPage({super.key});

  @override
  State<ActivateCardPage> createState() => _ActivateCardPageState();
}

class _ActivateCardPageState extends State<ActivateCardPage> {
  final TextEditingController _phoneController = TextEditingController();

  void _showSnack(String text) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(text)),
    );
  }

  Future<void> _submit() async {
    final text = _phoneController.text.trim();
    final valid = RegExp(r'^\d{9}$').hasMatch(text);

    if (!valid) {
      _showSnack('Número de telefone inválido (use 9 dígitos).');
      return;
    }

    // ---- LOADING ----
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => const Center(child: CircularProgressIndicator()),
    );

    // ---- PEDIDO À API ----
    final bool ok = await api.activateCurrentCard(text);

    Navigator.of(context).pop(); // fechar o loading

    if (!ok) {
      _showSnack('Não foi possível ativar o cartão.');
      return;
    }

    // ---- SUCESSO → página de cartão ativado ----
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (_) => const CardActivatedPage(),
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Activate card'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.all(24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              TextField(
                controller: _phoneController,
                keyboardType: TextInputType.number,
                inputFormatters: [
                  FilteringTextInputFormatter.digitsOnly,
                  LengthLimitingTextInputFormatter(9),
                ],
                decoration: InputDecoration(
                  labelText: 'Phone',
                  hintText: 'Enter phone',
                  prefixIcon: const Icon(Icons.phone),
                  border: OutlineInputBorder(
                    borderRadius: BorderRadius.circular(8),
                  ),
                  filled: true,
                  fillColor: Colors.white,
                ),
              ),
              const SizedBox(height: 20),
              ElevatedButton(
                onPressed: _submit,
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.green,
                  padding: const EdgeInsets.symmetric(
                    horizontal: 36,
                    vertical: 12,
                  ),
                ),
                child: const Text('Activate'),
              ),
            ],
          ),
        ),
      ),
    );
  }

  @override
  void dispose() {
    _phoneController.dispose();
    super.dispose();
  }
}
