import 'package:flutter/material.dart';
import 'api_controller.dart';
import 'activate_card_page.dart';
import 'card_deactivated_page.dart';

class CardActionsPage extends StatelessWidget {
  const CardActionsPage({super.key});

  void _showSnack(BuildContext context, String text) {
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(text)),
    );
  }

  /* Future<void> _onValidateExit(BuildContext context) async {
    // mostrar loading
    showDialog(
      context: context,
      barrierDismissible: false,
      builder: (_) => const Center(child: CircularProgressIndicator()),
    );

    final bool ok = await api.validateExit();

    Navigator.of(context).pop(); // fecha o loading

    if (!ok) {
      _showSnack(context, 'Não foi possível validar a saída deste cartão.');
      return;
    }

    // se OK → ir para página de cartão desativado
    Navigator.push(
      context,
      MaterialPageRoute(
        builder: (_) => const CardDeactivatedPage(),
      ),
    );
  } */


 Future<void> _onValidateExit(BuildContext context) async {
  showDialog(
    context: context,
    barrierDismissible: false,
    builder: (_) => const Center(child: CircularProgressIndicator()),
  );

  final result = await api.validateExitWithDebug();

  Navigator.of(context).pop(); // fecha loading

  // MOSTRAR DEBUG
  showDialog(
    context: context,
    builder: (ctx) => AlertDialog(
      title: const Text('Debug Info'),
      content: SingleChildScrollView(
        child: SelectableText(result['debug'] as String),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(ctx),
          child: const Text('OK'),
        ),
      ],
    ),
  );

  bool ok = result['success'] as bool;

  if (!ok) {
    _showSnack(context, 'Não foi possível validar a saída deste cartão.');
    return;
  }

  Navigator.push(
    context,
    MaterialPageRoute(
      builder: (_) => const CardDeactivatedPage(),
    ),
  );
}

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Card Actions'),
        backgroundColor: Colors.green,
      ),
      body: Center(
        child: Padding(
          padding: const EdgeInsets.symmetric(horizontal: 24.0),
          child: Column(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              ElevatedButton(
                onPressed: () {
                  // aqui ainda NÃO chamamos API, só vamos para a página
                  // onde o utilizador insere telefone e confirma
                  Navigator.push(
                    context,
                    MaterialPageRoute(
                      builder: (_) => const ActivateCardPage(),
                    ),
                  );
                },
                style: ElevatedButton.styleFrom(
                  minimumSize: const Size.fromHeight(50),
                  backgroundColor: Colors.green,
                ),
                child: const Text('Activate card'),
              ),
              const SizedBox(height: 16),
              ElevatedButton(
                onPressed: () => _onValidateExit(context),
                style: ElevatedButton.styleFrom(
                  minimumSize: const Size.fromHeight(50),
                  backgroundColor: Colors.green,
                ),
                child: const Text('Validate exit'),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
