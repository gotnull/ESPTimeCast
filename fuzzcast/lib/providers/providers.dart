
import 'package:flutter_riverpod/flutter_riverpod.dart';
import 'package:fuzzcast/api/api_service.dart';
import 'package:fuzzcast/models/config.dart';

final apiServiceProvider = Provider<ApiService>((ref) => ApiService());

final deviceIpProvider = StateNotifierProvider<DeviceIpNotifier, AsyncValue<String?>>((ref) {
  return DeviceIpNotifier(ref.read(apiServiceProvider));
});

class DeviceIpNotifier extends StateNotifier<AsyncValue<String?>> {
  final ApiService _apiService;

  DeviceIpNotifier(this._apiService) : super(const AsyncValue.loading()) {
    _loadDeviceIp();
  }

  Future<void> _loadDeviceIp() async {
    try {
      final ip = await _apiService.getDeviceIp();
      state = AsyncValue.data(ip);
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }

  Future<void> setDeviceIp(String ip) async {
    state = const AsyncValue.loading();
    try {
      await _apiService.setDeviceIp(ip);
      state = AsyncValue.data(ip);
    } catch (e, st) {
      state = AsyncValue.error(e, st);
    }
  }
}

final configProvider = FutureProvider<Config>((ref) async {
  final apiService = ref.read(apiServiceProvider);
  final ip = await ref.watch(deviceIpProvider.notifier)._apiService.getDeviceIp();
  if (ip == null) {
    throw Exception('Device IP not set. Please configure in settings.');
  }
  return await apiService.getConfig();
});

final timeProvider = FutureProvider<Map<String, dynamic>>((ref) async {
  final apiService = ref.read(apiServiceProvider);
  final ip = await ref.watch(deviceIpProvider.notifier)._apiService.getDeviceIp();
  if (ip == null) {
    throw Exception('Device IP not set. Please configure in settings.');
  }
  return apiService.getTime();
});

final forecastProvider = FutureProvider<Map<String, dynamic>>((ref) async {
  final apiService = ref.read(apiServiceProvider);
  final ip = await ref.watch(deviceIpProvider.notifier)._apiService.getDeviceIp();
  if (ip == null) {
    throw Exception('Device IP not set. Please configure in settings.');
  }
  return apiService.getForecast();
});
