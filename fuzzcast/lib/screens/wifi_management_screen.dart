import 'package:flutter/material.dart';
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fuzzcast/api/api_service.dart';
import 'package:fuzzcast/providers/providers.dart';

class WiFiManagementScreen extends ConsumerStatefulWidget {
  const WiFiManagementScreen({super.key});

  @override
  ConsumerState<WiFiManagementScreen> createState() => _WiFiManagementScreenState();
}

class _WiFiManagementScreenState extends ConsumerState<WiFiManagementScreen> {
  final TextEditingController _ssidController = TextEditingController();
  final TextEditingController _passwordController = TextEditingController();
  final ApiService _apiService = ApiService();

  List<WifiNetwork> _availableNetworks = [];
  WifiNetwork? _selectedNetwork;
  bool _isScanning = false;
  bool _isUpdating = false;
  bool _showPassword = false;
  String? _statusMessage;

  @override
  void initState() {
    super.initState();
    _loadCurrentConfig();
  }

  void _loadCurrentConfig() {
    final config = ref.read(configProvider).value;
    if (config != null) {
      _ssidController.text = config.ssid ?? '';
      _passwordController.text = config.password ?? '';
    }
  }

  Future<void> _scanNetworks() async {
    setState(() {
      _isScanning = true;
      _statusMessage = 'Scanning for WiFi networks...';
      _availableNetworks = []; // Clear previous results
    });

    try {
      final networks = await _apiService.scanNetworks().timeout(
        const Duration(seconds: 10),
        onTimeout: () {
          throw Exception('Scan timeout - device may not be reachable');
        },
      );

      if (mounted) {
        setState(() {
          _availableNetworks = networks;
          _isScanning = false;
          _statusMessage = 'Found ${networks.length} networks';
        });
      }
    } catch (e) {
      if (mounted) {
        setState(() {
          _isScanning = false;
          _statusMessage = 'Error scanning networks: $e';
        });
      }
    }
  }

  Future<void> _updateWiFiCredentials() async {
    if (_ssidController.text.isEmpty) {
      ScaffoldMessenger.of(context).showSnackBar(
        const SnackBar(content: Text('Please enter WiFi SSID')),
      );
      return;
    }

    setState(() {
      _isUpdating = true;
      _statusMessage = 'Updating WiFi credentials...';
    });

    try {
      await _apiService.updateWiFiCredentials(
        _ssidController.text,
        _passwordController.text,
      );

      setState(() {
        _isUpdating = false;
        _statusMessage = 'WiFi credentials updated successfully! Device will reconnect.';
      });

      // Refresh the config to show updated values
      ref.invalidate(configProvider);

      // Show success message
      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          const SnackBar(
            content: Text('WiFi credentials updated! Device is reconnecting...'),
            backgroundColor: Colors.green,
          ),
        );
      }

    } catch (e) {
      setState(() {
        _isUpdating = false;
        _statusMessage = 'Error updating WiFi credentials: $e';
      });

      if (mounted) {
        ScaffoldMessenger.of(context).showSnackBar(
          SnackBar(content: Text('Error updating WiFi: $e')),
        );
      }
    }
  }

  Widget _buildSignalStrengthIcon(int strength) {
    return Icon(
      strength >= 4 ? Icons.signal_wifi_4_bar
        : strength >= 3 ? Icons.network_wifi_3_bar
        : strength >= 2 ? Icons.network_wifi_2_bar
        : Icons.network_wifi_1_bar,
      color: strength >= 3 ? Colors.green : strength >= 2 ? Colors.orange : Colors.red,
      size: 20,
    );
  }

  @override
  Widget build(BuildContext context) {
    final configAsyncValue = ref.watch(configProvider);

    return Scaffold(
      appBar: AppBar(
        title: const Text('WiFi Management'),
      ),
      body: SingleChildScrollView(
        padding: const EdgeInsets.all(16.0),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.stretch,
          children: [
            // Current WiFi Status
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'Current WiFi Connection',
                      style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18),
                    ),
                    const SizedBox(height: 8),
                    configAsyncValue.when(
                      data: (config) => Column(
                        crossAxisAlignment: CrossAxisAlignment.start,
                        children: [
                          Text('SSID: ${config.ssid ?? 'Not set'}'),
                          const SizedBox(height: 4),
                          Row(
                            children: [
                              Text('Password: ${config.password?.isNotEmpty == true ? '••••••••' : 'Not set'}'),
                              IconButton(
                                icon: Icon(_showPassword ? Icons.visibility_off : Icons.visibility),
                                onPressed: () {
                                  setState(() {
                                    _showPassword = !_showPassword;
                                  });
                                },
                              ),
                            ],
                          ),
                          if (_showPassword && config.password?.isNotEmpty == true)
                            Text('Actual: ${config.password}', style: const TextStyle(fontSize: 12, color: Colors.grey)),
                        ],
                      ),
                      loading: () => const CircularProgressIndicator(),
                      error: (err, stack) => Text('Error: $err'),
                    ),
                  ],
                ),
              ),
            ),

            const SizedBox(height: 16),

            // Network Scanning
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'Available Networks',
                      style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18),
                    ),
                    const SizedBox(height: 8),
                    ElevatedButton(
                      onPressed: _isScanning ? null : _scanNetworks,
                      child: _isScanning
                        ? const SizedBox(
                            width: 16,
                            height: 16,
                            child: CircularProgressIndicator(strokeWidth: 2),
                          )
                        : const Text('Scan Networks'),
                    ),
                    const SizedBox(height: 8),
                    if (_availableNetworks.isNotEmpty) ...[
                      const Text('Available Networks:'),
                      const SizedBox(height: 8),
                      Container(
                        constraints: const BoxConstraints(maxHeight: 250),
                        decoration: BoxDecoration(
                          border: Border.all(color: Colors.grey.shade300),
                          borderRadius: BorderRadius.circular(8),
                        ),
                        child: ListView.builder(
                          shrinkWrap: true,
                          itemCount: _availableNetworks.length,
                          itemBuilder: (context, index) {
                            final network = _availableNetworks[index];
                            final isSelected = _selectedNetwork == network;
                            return ListTile(
                              dense: true,
                              leading: _buildSignalStrengthIcon(network.signalStrength),
                              title: Text(network.ssid),
                              subtitle: Text('${network.rssi} dBm'),
                              trailing: Row(
                                mainAxisSize: MainAxisSize.min,
                                children: [
                                  if (network.encrypted)
                                    const Icon(Icons.lock, size: 16)
                                  else
                                    const Icon(Icons.lock_open, size: 16),
                                  const SizedBox(width: 8),
                                  Icon(
                                    isSelected
                                      ? Icons.radio_button_checked
                                      : Icons.radio_button_unchecked,
                                    color: isSelected
                                      ? Theme.of(context).primaryColor
                                      : null,
                                  ),
                                ],
                              ),
                              onTap: () {
                                setState(() {
                                  _selectedNetwork = network;
                                  _ssidController.text = network.ssid;
                                });
                              },
                            );
                          },
                        ),
                      ),
                    ],
                  ],
                ),
              ),
            ),

            const SizedBox(height: 16),

            // WiFi Credentials Form
            Card(
              child: Padding(
                padding: const EdgeInsets.all(16.0),
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    const Text(
                      'WiFi Credentials',
                      style: TextStyle(fontWeight: FontWeight.bold, fontSize: 18),
                    ),
                    const SizedBox(height: 16),
                    TextField(
                      controller: _ssidController,
                      decoration: const InputDecoration(
                        labelText: 'WiFi SSID',
                        hintText: 'Enter network name',
                        border: OutlineInputBorder(),
                      ),
                    ),
                    const SizedBox(height: 16),
                    TextField(
                      controller: _passwordController,
                      decoration: InputDecoration(
                        labelText: 'WiFi Password',
                        hintText: 'Enter network password',
                        border: const OutlineInputBorder(),
                        suffixIcon: IconButton(
                          icon: Icon(_showPassword ? Icons.visibility_off : Icons.visibility),
                          onPressed: () {
                            setState(() {
                              _showPassword = !_showPassword;
                            });
                          },
                        ),
                      ),
                      obscureText: !_showPassword,
                    ),
                    const SizedBox(height: 16),
                    SizedBox(
                      width: double.infinity,
                      child: ElevatedButton(
                        onPressed: _isUpdating ? null : _updateWiFiCredentials,
                        style: ElevatedButton.styleFrom(
                          backgroundColor: Colors.blue,
                          foregroundColor: Colors.white,
                        ),
                        child: _isUpdating
                          ? const SizedBox(
                              width: 16,
                              height: 16,
                              child: CircularProgressIndicator(strokeWidth: 2),
                            )
                          : const Text('Update WiFi Credentials'),
                      ),
                    ),
                  ],
                ),
              ),
            ),

            const SizedBox(height: 16),

            // Status Message
            if (_statusMessage != null)
              Card(
                color: Theme.of(context).colorScheme.surfaceContainer,
                child: Padding(
                  padding: const EdgeInsets.all(16.0),
                  child: Text(
                    _statusMessage!,
                    style: Theme.of(context).textTheme.bodyMedium,
                  ),
                ),
              ),
          ],
        ),
      ),
    );
  }

  @override
  void dispose() {
    _ssidController.dispose();
    _passwordController.dispose();
    super.dispose();
  }
}