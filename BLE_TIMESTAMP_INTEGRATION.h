/**
 * @file BLE TIMESTAMP SYNCHRONIZATION
 * @brief Instrukcja integracji synchronizacji czasu przez BLE
 * 
 * OVERVIEW
 * ========
 * Urządzenie ESP32 odbiera timestamp (Unix epoch) z telefonu przez BLE
 * zaraz po połączeniu. To gwarantuje, że wszystkie pomiary mają prawidłowy
 * czas niezależnie od stanu WiFi.
 * 
 * CHARAKTERYSTYKA BLE
 * ===================
 * UUID Service:  0x00FF
 * UUID Char:     0xFF0A (CHAR_TIMESTAMP_UUID)
 * Permissions:   WRITE
 * Format:        uint32_t (4 bajty, little-endian)
 * Content:       Unix timestamp (sekundy od 1970-01-01)
 * 
 * SEKWENCJA ZDARZEŃ
 * =================
 * 1. ESP32 startuje → BLE advertising
 * 2. Telefon skanuje i łączy się → BLE CONNECT
 * 3. Telefon odczytuje dostępne charakterystyki
 * 4. Telefon wysyła timestamp do 0xFF0A
 * 5. ESP32 odbiera → sntp_client_set_timestamp(timestamp)
 * 6. System time zostaje zaktualizowany
 * 7. Wszystkie pomiary zbierane od tego momentu mają prawidłowy czas
 * 
 * IMPLEMENTACJA - PYTHON/ANDROID
 * ===============================
 * 
 * import time
 * import struct
 * 
 * # Pobierz aktualny Unix timestamp
 * timestamp = int(time.time())
 * 
 * # Konwertuj na 4 bajty (little-endian)
 * timestamp_bytes = struct.pack('<I', timestamp)  # <I = unsigned int, little-endian
 * 
 * # Wyślij do charakterystyki 0xFF0A
 * ble_device.write_gatt_char(
 *     uuid='0000ff0a-0000-1000-8000-00805f9b34fb',  # Full UUID
 *     data=timestamp_bytes
 * )
 * 
 * IMPLEMENTACJA - JAVASCRIPT/REACT NATIVE
 * ========================================
 * 
 * import { BleManager } from 'react-native-ble-plx';
 * 
 * const writeTimestamp = async (device) => {
 *   const timestamp = Math.floor(Date.now() / 1000);  // Unix timestamp
 *   const buffer = Buffer.alloc(4);
 *   buffer.writeUInt32LE(timestamp, 0);  // Little-endian
 *   
 *   await device.writeCharacteristicWithoutResponseForService(
 *     '000000ff-0000-1000-8000-00805f9b34fb',  // Service UUID
 *     '0000ff0a-0000-1000-8000-00805f9b34fb',  // Char UUID 0xFF0A
 *     buffer.toString('base64')
 *   );
 *   
 *   console.log('Timestamp synced:', timestamp);
 * }
 * 
 * IMPLEMENTACJA - iOS/Swift
 * =========================
 * 
 * let timestamp = UInt32(Date().timeIntervalSince1970)
 * var timestampBytes: [UInt8] = []
 * timestampBytes.append(UInt8(timestamp & 0xFF))
 * timestampBytes.append(UInt8((timestamp >> 8) & 0xFF))
 * timestampBytes.append(UInt8((timestamp >> 16) & 0xFF))
 * timestampBytes.append(UInt8((timestamp >> 24) & 0xFF))
 * 
 * let data = Data(timestampBytes)
 * peripheral.writeValue(data, for: characteristic, type: .withoutResponse)
 * 
 * FALLBACK
 * ========
 * Jeśli telefon nie wyśle timestampu (np. utracone połączenie BLE),
 * urządzenie powróci do fallback systemu:
 * - timestamp = BUILD_TIMESTAMP + uptime (esp_timer_get_time())
 * 
 * To gwarantuje ciągłość pomiarów, chociaż mogą mieć błąd czasowy.
 * 
 * WERYFIKACJA
 * ===========
 * Po wysłaniu timestampu powinieneś zobaczyć w seryjnym porcie:
 * I (timestamp) SNTP_CLIENT: Timestamp synced via BLE: 1706558400
 * 
 * DEBUG
 * =====
 * - Sprawdź czy charakterystyka 0xFF0A jest dostępna (WRITE permission)
 * - Sprawdź czy dane mają dokładnie 4 bajty
 * - Sprawdź czy format to little-endian (uint32_t)
 */
