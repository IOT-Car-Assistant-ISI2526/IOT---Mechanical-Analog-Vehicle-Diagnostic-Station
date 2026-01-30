# BLE Timestamp Synchronization - Instrukcja dla aplikacji telefonu

## 📋 Przegląd

Po połączeniu z ESP32 przez BLE, aplikacja telefonu powinna **natychmiast wysłać timestamp** (Unix epoch) do urządzenia. To gwarantuje, że wszystkie pomiary zbierane przez ESP32 mają prawidłowy czas, niezależnie od dostępności WiFi i NTP.

---

## 🎯 Charakterystyka BLE

| Parametr | Wartość |
|----------|---------|
| **Service UUID** | `0x00FF` |
| **Characteristic UUID** | `0xFF0A` |
| **Permissions** | WRITE (bez response) |
| **Data Format** | `uint32_t` (4 bajty, little-endian) |
| **Wartość** | Unix timestamp (sekundy od 1970-01-01) |

---

## 🔄 Sekwencja połączenia

```
1. ESP32 startuje → Broadcast BLE
2. sntp_client_init() → Odczytuje ostatni timestamp z NVS
3. Telefon skanuje sieci
4. Telefon wybiera "ESP32-NotesWiFi"
5. BLE CONNECT ✓
6. Telefon wysyła: write(0xFF0A, timestamp_bytes)
7. ESP32 odbiera: sntp_client_set_timestamp(timestamp)
8. Timestamp zapisany do NVS ✓
9. System time zaktualizowany ✓
10. Pomiary zbierane z prawidłowym czasem ✓
```

**Po resecie urządzenia:**
```
1. ESP32 restart
2. sntp_client_init() → Odczytuje ostatni timestamp z NVS
3. Bazowy czas = ostatni_timestamp + uptime
4. Pomiary kontynuowane z przybliżonym czasem
5. Po ponownym BLE connect → nowy timestamp nadpisuje stary
```

---

## 💻 Implementacja - Język/Framework

### **Python (PC/Linux)**

```python
import time
import struct
from bleak import BleakClient

async def sync_timestamp(device_address):
    SERVICE_UUID = "0000ff00-0000-1000-8000-00805f9b34fb"
    CHAR_TIMESTAMP_UUID = "0000ff0a-0000-1000-8000-00805f9b34fb"
    
    async with BleakClient(device_address) as client:
        # Pobierz aktualny Unix timestamp
        timestamp = int(time.time())
        
        # Konwertuj na 4 bajty (little-endian)
        timestamp_bytes = struct.pack('<I', timestamp)
        
        # Wyślij do ESP32
        await client.write_gatt_char(CHAR_TIMESTAMP_UUID, timestamp_bytes)
        
        print(f"✓ Timestamp synced: {timestamp}")
```

### **React Native / Expo**

```javascript
import { BleManager } from 'react-native-ble-plx';

const bleManager = new BleManager();

async function syncTimestamp(device) {
  const SERVICE_UUID = '0000ff00-0000-1000-8000-00805f9b34fb';
  const CHAR_UUID = '0000ff0a-0000-1000-8000-00805f9b34fb';
  
  try {
    const timestamp = Math.floor(Date.now() / 1000);
    const buffer = Buffer.alloc(4);
    buffer.writeUInt32LE(timestamp, 0);  // Little-endian
    
    await device.writeCharacteristicWithoutResponseForService(
      SERVICE_UUID,
      CHAR_UUID,
      buffer.toString('base64')
    );
    
    console.log('✓ Timestamp synced:', timestamp);
  } catch (error) {
    console.error('Failed to sync timestamp:', error);
  }
}

// Użycie po połączeniu:
device.onConnected(() => {
  syncTimestamp(device);
});
```

### **Flutter/Dart**

```dart
import 'package:flutter_blue/flutter_blue.dart';

void syncTimestamp(BluetoothDevice device) {
  final serviceUuid = Guid('0000ff00-0000-1000-8000-00805f9b34fb');
  final charUuid = Guid('0000ff0a-0000-1000-8000-00805f9b34fb');
  
  device.discoverServices().then((services) {
    final service = services.firstWhere(
      (s) => s.uuid == serviceUuid,
      orElse: () => null,
    );
    
    if (service != null) {
      final char = service.characteristics.firstWhere(
        (c) => c.uuid == charUuid,
        orElse: () => null,
      );
      
      if (char != null) {
        final timestamp = DateTime.now().millisecondsSinceEpoch ~/ 1000;
        final bytes = [
          timestamp & 0xFF,
          (timestamp >> 8) & 0xFF,
          (timestamp >> 16) & 0xFF,
          (timestamp >> 24) & 0xFF,
        ];
        
        char.write(bytes);
        print('✓ Timestamp synced: $timestamp');
      }
    }
  });
}
```

### **iOS/Swift**

```swift
import CoreBluetooth

func syncTimestamp(to characteristic: CBCharacteristic, peripheral: CBPeripheral) {
    let timestamp = UInt32(Date().timeIntervalSince1970)
    
    // Konwertuj na 4 bajty (little-endian)
    var timestampBytes: [UInt8] = []
    timestampBytes.append(UInt8(timestamp & 0xFF))
    timestampBytes.append(UInt8((timestamp >> 8) & 0xFF))
    timestampBytes.append(UInt8((timestamp >> 16) & 0xFF))
    timestampBytes.append(UInt8((timestamp >> 24) & 0xFF))
    
    let data = Data(timestampBytes)
    peripheral.writeValue(
        data,
        for: characteristic,
        type: .withoutResponse  // Bez oczekiwania na potwierdzenie
    )
    
    print("✓ Timestamp synced: \(timestamp)")
}
```

### **Android/Kotlin**

```kotlin
import android.bluetooth.BluetoothGattCharacteristic
import java.nio.ByteBuffer
import java.nio.ByteOrder

fun syncTimestamp(characteristic: BluetoothGattCharacteristic, gatt: BluetoothGatt) {
    val timestamp = (System.currentTimeMillis() / 1000).toInt()
    
    // Konwertuj na bajty (little-endian)
    val buffer = ByteBuffer.allocate(4)
    buffer.order(ByteOrder.LITTLE_ENDIAN)
    buffer.putInt(timestamp)
    
    characteristic.value = buffer.array()
    characteristic.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
    
    gatt.writeCharacteristic(characteristic)
    
    Log.d("BLE", "Timestamp synced: $timestamp")
}
```

---

## ✅ Weryfikacja

Po pomyślnym wysłaniu timestampu powinieneś zobaczyć w **serial monitor** ESP32:

```
I (12345) SNTP_CLIENT: Timestamp synced via BLE: 1706558400
I (12346) BLE_SERVER: Timestamp synced via BLE: 1706558400
```

---

## ⚡ Optymalne czasy

| Faza | Opis |
|------|------|
| **Zaraz po BLE CONNECT** | ⭐⭐⭐ **Idealne!** Wysyłaj timestamp natychmiast |
| **W UI init** | ⭐⭐ Późno, ale lepiej niż nigdy |
| **Periodic (co 60s)** | ⚠️ Zbędne, ale nie będzie kłopotów |

---

## 🔙 Fallback

System automatycznie wybiera najlepszy dostępny czas:

1. **BLE timestamp (aktualna sesja)** → ⭐⭐⭐ Najdokładniejszy  
   `return time(NULL)` - systemowy czas zaktualizowany przez BLE

2. **Ostatni timestamp z NVS + uptime** → ⭐⭐ Przybliżony  
   `return last_timestamp + uptime` - wykorzystuje ostatni znany czas

3. **BUILD_TIMESTAMP + uptime** → ⭐ Fallback awaryjny  
   `return BUILD_TIMESTAMP + uptime` - tylko jeśli nigdy nie było BLE sync

**Przykład:**
- ESP32 otrzymał timestamp przez BLE: `1706558400` (30 stycznia 2026, 10:00)
- Zapisano w NVS ✓
- ESP32 restart po 1 godzinie
- Odczyt z NVS: `1706558400`
- Uptime: `3600s`
- Aktualny czas: `1706558400 + 3600 = 1706562000` (11:00) ✓

To daje **ciągłość czasu między resetami** bez potrzeby połączenia WiFi/BLE!

---

## 🐛 Troubleshooting

| Problem | Przyczyna | Rozwiązanie |
|---------|-----------|------------|
| "Characteristic not found" | Brak UUID 0xFF0A | Sprawdź czy urządzenie to `ESP32-NotesWiFi` |
| "Write failed" | Brak WRITE permission | Upewnij się że piszesz do 0xFF0A (nie do innej karakterystyki) |
| "Data length mismatch" | Wysłano != 4 bajty | Sprawdź czy dane mają dokładnie 4 bajty |
| Timestamp nie zsynchronizowany | Zła kolejność bajtów | Sprawdź czy używasz **little-endian** (< w struct.pack) |

---

## 📝 Testowanie

### Test lokalny (Python)

```bash
pip install bleak

# scan.py
import asyncio
from bleak import BleakScanner

async def main():
    devices = await BleakScanner.discover()
    for d in devices:
        if "ESP32" in d.name:
            print(f"Found: {d.address} - {d.name}")

asyncio.run(main())
```

---

## ❓ FAQ

**P: Czy muszę wysyłać timestamp co kilka sekund?**
O: Nie! Wysłań raz po connect. ESP32 zapamiętuje czas dzięki systemowemu timingowi.

**P: Co jeśli wysłam zły timestamp?**
O: Wszystkie pomiary będą miały błąd o tę różnicę. Wysyłaj zawsze `time.time()` bez zmian.

**P: Czy działa offline?**
O: Tak! Raz zsynchronizowany czas trwale zostaje w ESP32 aż do resetu sprzętu.

**P: Czy mogę wysłać zero lub negatywną liczbę?**
O: Nie! To spowoduje błąd. Zawsze wysyłaj `int(time.time())` (> 1000000000 w 2001).

---

## 📚 Referencje

- [Unix Time](https://en.wikipedia.org/wiki/Unix_time)
- [ESP32 time() function](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/system/system_time.html)
- [settimeofday()](https://pubs.opengroup.org/onlinepubs/9699919799/functions/gettimeofday.html)
