# SD Card Timeout Issue - Analiza i Rozwiązania

## 🔴 Problem

```
E (792) sdmmc_common: sdmmc_init_ocr: send_op_cond (1) returned 0x107
E (792) vfs_fat_sdmmc: sdmmc_card_init failed (0x107).
ESP_ERR_TIMEOUT - błąd timeout SD init
```

Błąd pojawia się **intermittentnie** (czasami po rebootcie lub reupload'ie innego kodu ustępuje).

---

## 🔍 Główne Przyczyny

### 1. **Konflikt SPI Bus** ⚠️ KRYTYCZNE
```c
// main.c - spi_initialize_master() dla MAX6675
spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

// storage_manager.c - SD card też na SPI2_HOST
spi_bus_initialize(SPI_HOST_USED, &bus_cfg, SPI_DMA_CH_AUTO);
// gdzie SPI_HOST_USED = SPI2_HOST
```

**Problem:** Oba urządzenia (MAX6675 + SD card) używają tego samego SPI2_HOST!
- Pierwsze initialize = OK
- Drugie initialize = `ESP_ERR_INVALID_STATE` (bus już w użyciu)
- SD card nie ma dostępu do bus = TIMEOUT

### 2. **Szybkość SPI za wysoka**
- `host.max_freq_khz = 4000` - może być za szybkie na początek
- SD karty wymagają wolniejszej inicjalizacji (~1 MHz)

### 3. **Brak delay'u między SPI init a SD init**
- SPI bus potrzebuje stabilizacji
- Brak delay = state machine niezabezpieczony

### 4. **Intermittentny charakter**
- Zmieniające się warunki zasilania
- Timing race conditions
- Kalibracja oscylatora

---

## ✅ Rozwiązania (już zaimplementowane)

### 1. **Retry logika**
```c
#define SD_INIT_RETRIES 3
#define SD_INIT_RETRY_DELAY_MS 500

for (retry_count = 0; retry_count < SD_INIT_RETRIES; retry_count++) {
    ret = esp_vfs_fat_sdspi_mount(...);
    if (ret == ESP_OK) {
        return;  // SUCCESS
    }
    vTaskDelay(pdMS_TO_TICKS(SD_INIT_RETRY_DELAY_MS));
}
```

**Efekt:** Jeśli timeout jest tymczasowy, powtórzenie zwykle się powiedzie.

### 2. **Zmniejszona szybkość inicjalizacyjna**
```c
// host.max_freq_khz = 4000;  // STARE
host.max_freq_khz = 1000;   // NOWE - bardziej stabilne
```

**Efekt:** SD card ma więcej czasu na odpowiedź.

### 3. **Delay po SPI init**
```c
ret = spi_bus_initialize(SPI_HOST_USED, &bus_cfg, SPI_DMA_CH_AUTO);
// Stabilizacja SPI przed SD init
vTaskDelay(pdMS_TO_TICKS(100));
```

**Efekt:** SPI bus się stabilizuje zanim SD spróbuje się podłączyć.

### 4. **Graceful Degradation**
```c
if (ret != ESP_OK) {
    ESP_LOGW(TAG, "Failed to mount SD card after %d attempts", SD_INIT_RETRIES);
    ESP_LOGW(TAG, "HINT: Please verify SD card is inserted. Continuing without SD card storage.");
    return;  // Nie abort! Kontynuuj pracę
}
```

**Efekt:** Jeśli SD nie zadziała, urządzenie pracuje dalej (np. bez buforowania na SD).

---

## 🔧 Długoterminowe Rozwiązania

### Opcja A: Oddzielne SPI bus (NAJLEPSZE)
```c
// MAX6675 na SPI2_HOST
spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

// SD card na SPI3_HOST (jeśli dostępny)
host.slot = SDSPI_SLOT_SPI3;  // Zamiast default SPI2
```

**Wymaga zmiany w hardware'u - dodatkowe piny?**

### Opcja B: Zmiana pinów SD card
Użyć innych pinów SPI dla SD:
```c
// Zmień PIN_NUM_MISO, PIN_NUM_MOSI, PIN_NUM_CLK dla SD
// aby były na innym SPI bus
```

**Wymaga sprawdzenia dostępnych pinów ESP32.**

### Opcja C: Reorder inicjalizacji
```c
// Zainicjalizuj SD PRZED MAX6675
storage_init();        // SD card first
spi_initialize_master(); // MAX6675 second
```

**Może pomóc, jeśli SD potrzebuje pełnego dostępu do bus na start.**

---

## 📋 Checklist Debugowania

- [ ] Sprawdzić czy SD card jest włożona i dobrze zainstalowana
- [ ] Sprawdzić połączenia pinów SPI (MISO, MOSI, SCK, CS)
- [ ] Sprawdzić zasilanie SD card (może być za słabe)
- [ ] Sprawdzić czy nie ma krótkich połączeń
- [ ] Sprawdzić czy piny nie są już w użyciu przez inne urządzenia
- [ ] Zarejestrować wydajność - czy timeout zawsze po 1s?
- [ ] Spróbować inną SD kartę (może być uszkodzona)

---

## 🎯 Oczekiwane Logi po Poprawce

### Sukces:
```
I (1234) STORAGE_MGR: SD card mount attempt 1/3
I (1250) STORAGE_MGR: SD card mounted successfully
I (1260) STORAGE_MGR: Free space: 8589934592 bytes
```

### Z retry:
```
I (1234) STORAGE_MGR: SD card mount attempt 1/3
W (1240) STORAGE_MGR: SD card mount failed (ESP_ERR_TIMEOUT), retry in 500ms
I (1740) STORAGE_MGR: SD card mount attempt 2/3
I (1756) STORAGE_MGR: SD card mounted successfully
```

### Graceful fail:
```
I (1234) STORAGE_MGR: SD card mount attempt 1/3
W (1240) STORAGE_MGR: SD card mount failed (ESP_ERR_TIMEOUT), retry in 500ms
I (1740) STORAGE_MGR: SD card mount attempt 2/3
W (1746) STORAGE_MGR: SD card mount failed (ESP_ERR_TIMEOUT), retry in 500ms
I (2246) STORAGE_MGR: SD card mount attempt 3/3
W (2252) STORAGE_MGR: SD card mount failed (ESP_ERR_TIMEOUT), retry in 500ms
W (2752) STORAGE_MGR: Failed to mount SD card after 3 attempts
W (2752) STORAGE_MGR: HINT: Please verify SD card is inserted. Continuing without SD card storage.
```

---

## 📞 Jeśli Problem Trwa

Jeśli timeout nadal się pojawia nawet z retry:

1. **Zmniej `host.max_freq_khz` dalej:**
   ```c
   host.max_freq_khz = 400;  // Ultra-slow init
   ```

2. **Zwiększ SD_INIT_RETRIES:**
   ```c
   #define SD_INIT_RETRIES 5
   #define SD_INIT_RETRY_DELAY_MS 1000
   ```

3. **Sprawdzić konflikt SPI:**
   ```bash
   # W main.c, zakomentuj spi_initialize_master()
   // spi_initialize_master(SPI_MISO_PIN, SPI_MOSI_PIN, SPI_SCK_PIN);
   # Czy SD card zadziała bez MAX6675?
   ```

4. **Włączyć debug SPI:**
   ```c
   esp_log_level_set("sdspi_host", ESP_LOG_DEBUG);
   esp_log_level_set("sdmmc_common", ESP_LOG_DEBUG);
   ```

---

## 📚 Referencje

- [ESP-IDF SD Card Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/sdspi_host.html)
- [SPI Bus Conflict Resolution](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/spi_master.html)
- [GPIO Allocation Guidelines](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/hw-reference/esp32_datasheet.pdf)
