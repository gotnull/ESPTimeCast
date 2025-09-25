import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fuzzcast/providers/providers.dart';

class ManualIpScreen extends ConsumerStatefulWidget {
  const ManualIpScreen({super.key});

  @override
  ConsumerState<ManualIpScreen> createState() => _ManualIpScreenState();
}

class _ManualIpScreenState extends ConsumerState<ManualIpScreen> {
  final TextEditingController _ipController = TextEditingController();
  bool _isConnecting = false;

  @override
  void initState() {
    super.initState();
    _loadCurrentIp();
  }

  void _loadCurrentIp() async {
    final apiService = ref.read(apiServiceProvider);
    final ip = await apiService.getDeviceIp();
    if (ip != null) {
      _ipController.text = ip;
    }
  }

  Future<void> _setDeviceIp() async {
    final ip = _ipController.text.trim();
    if (ip.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please enter an IP address')),
      );
      return;
    }

    setState(() {
      _isConnecting = true;
    });

    try {
      // Set the IP
      await ref.read(deviceIpProvider.notifier).setDeviceIp(ip);

      // Test connection by trying to get config
      final apiService = ref.read(apiServiceProvider);
      await apiService.getConfig();

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('Connected successfully!'),
            backgroundColor: Colors.green,
          ),
        );
        Navigator.of(context).pop();
      }
    } catch (e) {
      setState(() {
        _isConnecting = false;
      });

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Connection failed: $e')),
        );
      }
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: const Text('Connect to Device'),
      ),
      body: Padding(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            const Icon(
              Icons.router,
              size: 80,
              color: Colors.blue,
            ),
            const SizedBox(height: 24),
            Text(
              'Enter Device IP Address',
              style: Theme.of(context).textTheme.headlineSmall,
              textAlign: TextAlign.center,
            ),
            const SizedBox(height: 8),
            const Text(
              'Find your device\'s IP address on your router or network scanner app',
              textAlign: TextAlign.center,
              style: TextStyle(color: Colors.grey),
            ),
            const SizedBox(height: 32),

            TextField(
              controller: _ipController,
              decoration: const InputDecoration(
                labelText: 'IP Address',
                hintText: 'e.g., 192.168.1.100',
                border: OutlineInputBorder(),
                prefixIcon: Icon(Icons.computer),
              ),
              keyboardType: const TextInputType.numberWithOptions(decimal: true),
              enabled: !_isConnecting,
            ),

            const SizedBox(height: 24),

            ElevatedButton.icon(
              onPressed: _isConnecting ? null : _setDeviceIp,
              icon: _isConnecting
                ? const SizedBox(
                    width: 16,
                    height: 16,
                    child: CircularProgressIndicator(strokeWidth: 2),
                  )
                : const Icon(Icons.connect_without_contact),
              label: Text(_isConnecting ? 'Connecting...' : 'Connect'),
              style: ElevatedButton.styleFrom(
                backgroundColor: Colors.blue,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 16),
              ),
            ),

            const SizedBox(height: 32),

            ExpansionTile(
              title: const Text('Common IP Address Ranges'),
              children: [
                ListTile(
                  title: const Text('192.168.1.x'),
                  subtitle: const Text('Most common home router range'),
                  trailing: TextButton(
                    onPressed: () => _ipController.text = '192.168.1.',
                    child: const Text('Use'),
                  ),
                ),
                ListTile(
                  title: const Text('192.168.0.x'),
                  subtitle: const Text('Alternative common range'),
                  trailing: TextButton(
                    onPressed: () => _ipController.text = '192.168.0.',
                    child: const Text('Use'),
                  ),
                ),
                ListTile(
                  title: const Text('10.0.0.x'),
                  subtitle: const Text('Some routers use this range'),
                  trailing: TextButton(
                    onPressed: () => _ipController.text = '10.0.0.',
                    child: const Text('Use'),
                  ),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _ipController.dispose();
    super.dispose();
  }
}