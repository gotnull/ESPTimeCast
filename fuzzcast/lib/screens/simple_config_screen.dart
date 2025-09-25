import 'package:flutter/material.dart';
import 'package:fuzzcast/api/api_service.dart';
import 'package:fuzzcast/models/config.dart';

class SimpleConfigScreen extends StatefulWidget {
  final String deviceIp;
  const SimpleConfigScreen({required this.deviceIp, super.key});

  @override
  State<SimpleConfigScreen> createState() => _SimpleConfigScreenState();
}

class _SimpleConfigScreenState extends State<SimpleConfigScreen> {
  final _apiKeyController = TextEditingController();
  final _cityController = TextEditingController();
  final _countryController = TextEditingController();
  final ApiService _api = ApiService();
  bool isLoading = false;

  @override
  void initState() {
    super.initState();
    _loadConfig();
  }

  Future<void> _loadConfig() async {
    setState(() {
      isLoading = true;
    });
    try {
      final config = await _api.getConfig();
      _apiKeyController.text = config.openWeatherApiKey ?? '';
      _cityController.text = config.openWeatherCity ?? '';
      _countryController.text = config.openWeatherCountry ?? '';
    } catch (e) {
      if (mounted) {
        debugPrint(e.toString());
        ScaffoldMessenger.of(
          context,
        ).showSnackBar(SnackBar(content: Text('Error loading config: $e')));
      }
    }
    setState(() {
      isLoading = false;
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(title: const Text('Device Config')),
      body: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          children: [
            TextField(
              controller: _apiKeyController,
              decoration: const InputDecoration(
                labelText: 'Weather API Key',
                hintText: 'c0a3e9da669747436668700ed1a8f07f',
                border: OutlineInputBorder(),
              ),
              contextMenuBuilder: (context, editableTextState) =>
                  const SizedBox.shrink(),
            ),
            const SizedBox(height: 16),

            TextField(
              controller: _cityController,
              decoration: const InputDecoration(
                labelText: 'City',
                hintText: 'Melbourne',
                border: OutlineInputBorder(),
              ),
              contextMenuBuilder: (context, editableTextState) =>
                  const SizedBox.shrink(),
            ),
            const SizedBox(height: 16),

            TextField(
              controller: _countryController,
              decoration: const InputDecoration(
                labelText: 'Country',
                hintText: 'AU',
                border: OutlineInputBorder(),
              ),
              contextMenuBuilder: (context, editableTextState) =>
                  const SizedBox.shrink(),
            ),
            const SizedBox(height: 24),

            SizedBox(
              width: double.infinity,
              child: ElevatedButton(
                onPressed: () async {
                  final navigator = Navigator.of(context);
                  final scaffoldMessenger = ScaffoldMessenger.of(context);

                  setState(() {
                    isLoading = true;
                  });

                  scaffoldMessenger.showSnackBar(
                    const SnackBar(
                      content: Text('UPDATING CONFIG...'),
                      backgroundColor: Colors.orange,
                    ),
                  );

                  try {
                    final newConfig = Config(
                      openWeatherApiKey: _apiKeyController.text,
                      openWeatherCity: _cityController.text,
                      openWeatherCountry: _countryController.text,
                    );
                    await _api.updateConfig(newConfig);

                    if (mounted) {
                      scaffoldMessenger.showSnackBar(
                        const SnackBar(
                          content: Text('CONFIG UPDATED!'),
                          backgroundColor: Colors.green,
                        ),
                      );

                      navigator.pop();
                    }
                  } catch (e) {
                    if (mounted) {
                      scaffoldMessenger.showSnackBar(
                        SnackBar(
                          content: Text('FAILED: $e'),
                          backgroundColor: Colors.red,
                        ),
                      );
                    }
                  }

                  if (mounted) {
                    setState(() {
                      isLoading = false;
                    });
                  }
                },
                style: ElevatedButton.styleFrom(
                  backgroundColor: Colors.red,
                  foregroundColor: Colors.white,
                ),
                child: isLoading
                    ? const CircularProgressIndicator(color: Colors.white)
                    : const Text('UPDATE CONFIG NOW'),
              ),
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _apiKeyController.dispose();
    _cityController.dispose();
    _countryController.dispose();
    super.dispose();
  }
}
