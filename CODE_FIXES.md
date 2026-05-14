# CN105 Codereview – Umfassende Lösungsvorschläge

## 📋 ÜBERSICHT

Alle Fixes in Prioritätsreihenfolge mit konkreten Code-Snippets.

---

## 🔴 KRITISCHE SICHERHEITSPROBLEME

### FIX 1: Unsafe const_cast entfernen
**Datei**: `components/cn105/hp_readings.cpp` (Zeile 38)

**Aktuell (UNSAFE)**:
```cpp
this->data = const_cast<uint8_t*>(this->parser_.data());
```

**Variante A: Data kopieren (SICHER, empfohlen)**:
```cpp
// Im Header cn105.h: Größe erhöhen falls nötig
uint8_t data[64];  // Match MAX_DATA_BYTES

// In hp_readings.cpp
if (this->parser_.data_length() <= 64) {
    memcpy(this->data, this->parser_.data(), this->parser_.data_length());
} else {
    ESP_LOGE(TAG, "Parser frame too large: %d > 64", this->parser_.data_length());
    return;
}
```

**Variante B: Parser Interface ändern**:
```cpp
// In frame_parser.h: Neuer non-const getter
class FrameParser {
public:
    uint8_t* data_mutable() { 
        return &buffer_[5]; 
    }
    // Bestehender const getter
    const uint8_t* data() const { return &buffer_[5]; }
};

// Dann in hp_readings.cpp:
this->data = this->parser_.data_mutable();
```

**Empfehlung**: Variante A (Kopieren) ist sicherer.

---

### FIX 2: Buffer Overflow – string statt char[256]
**Datei**: `components/cn105/heatpumpFunctions.cpp` (Zeile 12-25)

**Aktuell (UNSAFE)**:
```cpp
void CN105Climate::functionsArrived() {
    char states[256];
    states[0] = '\0';
    size_t remaining = sizeof(states);
    char* pos = states;

    heatpumpFunctionCodes codes = functions.getAllCodes();
    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        if (codes.valid[i]) {
            int code = codes.code[i];
            int value = functions.getValue(code);
            if (value > 0) {
                int written = snprintf(pos, remaining, "%i: %i ", code, value);
                if (written < 0 || static_cast<size_t>(written) >= remaining) {
                    break;
                }
                pos += written;
                remaining -= written;
            }
        }
    }
    if (this->Functions_sensor_ != nullptr) {
        this->Functions_sensor_->publish_state(states);
    }
}
```

**FIX (SAFE)**:
```cpp
void CN105Climate::functionsArrived() {
    std::string states;
    
    heatpumpFunctionCodes codes = functions.getAllCodes();
    for (int i = 0; i < MAX_FUNCTION_CODE_COUNT; ++i) {
        if (codes.valid[i]) {
            int code = codes.code[i];
            int value = functions.getValue(code);
            if (value > 0) {
                if (!states.empty()) {
                    states += " ";
                }
                states += std::to_string(code) + ": " + std::to_string(value);
            }
        }
    }
    
    // Safety check: string size limit
    if (states.length() > 1024) {
        ESP_LOGW(LOG_HARDWARE_SELECT_TAG, "Functions string truncated (too long)");
        states.resize(1024);
    }
    
    if (this->Functions_sensor_ != nullptr) {
        this->Functions_sensor_->publish_state(states);
    }
    
    // Update Hardware Settings Selects
    for (auto* setting : this->hardware_settings_) {
        int val = functions.getValue(setting->get_code());
        if (val > 0) {
            setting->update_state_from_value(val);
        } else {
            ESP_LOGD(LOG_HARDWARE_SELECT_TAG, "Code %d received unknown value: %d", 
                     setting->get_code(), val);
        }
    }
}
```

---

### FIX 3: Mutex Race Condition – Unified Thread Safety
**Datei**: `components/cn105/cn105.h`

**Aktuell (UNSAFE auf ESP8266)**:
```cpp
#ifdef USE_ESP32
    std::mutex esp32Mutex;
#else
    void testEmulateMutex(const char* retryName, std::function<void()>&& f);
    bool esp8266Mutex = false;
#endif
```

**FIX 1: Wrapper Class mit RAII**:
```cpp
// Neu in cn105.h

class SettingsGuard {
private:
    CN105Climate* parent_;
    bool locked_;
    
public:
    explicit SettingsGuard(CN105Climate* parent) : parent_(parent), locked_(false) {
        if (parent_) {
            parent_->lock_wanted_settings();
            locked_ = true;
        }
    }
    
    ~SettingsGuard() {
        if (parent_ && locked_) {
            parent_->unlock_wanted_settings();
            locked_ = false;
        }
    }
    
    // Prevent copying
    SettingsGuard(const SettingsGuard&) = delete;
    SettingsGuard& operator=(const SettingsGuard&) = delete;
    
    // Allow moving
    SettingsGuard(SettingsGuard&& other) noexcept 
        : parent_(other.parent_), locked_(other.locked_) {
        other.parent_ = nullptr;
        other.locked_ = false;
    }
};
```

**FIX 2: In cn105.h – Member-Deklarationen**:
```cpp
private:
#ifdef USE_ESP32
    std::mutex settings_mutex_;
#else
    volatile uint8_t settings_lock_count_ = 0;
    static constexpr uint32_t LOCK_TIMEOUT_MS = 100;
    uint32_t lock_start_ms_ = 0;
#endif
    
public:
    void lock_wanted_settings();
    void unlock_wanted_settings();
    bool is_wanted_settings_locked() const;
```

**FIX 3: In cn105.cpp – Implementierung**:
```cpp
void CN105Climate::lock_wanted_settings() {
#ifdef USE_ESP32
    settings_mutex_.lock();
#else
    // Emulate mutex mit timeout
    uint32_t start = CUSTOM_MILLIS;
    while (this->settings_lock_count_ > 0) {
        if (CUSTOM_MILLIS - start > LOCK_TIMEOUT_MS) {
            ESP_LOGW("MUTEX", "Settings lock timeout after %u ms", LOCK_TIMEOUT_MS);
            break;
        }
        delayMicroseconds(10);
    }
    this->settings_lock_count_++;
    this->lock_start_ms_ = CUSTOM_MILLIS;
#endif
}

void CN105Climate::unlock_wanted_settings() {
#ifdef USE_ESP32
    settings_mutex_.unlock();
#else
    if (this->settings_lock_count_ > 0) {
        this->settings_lock_count_--;
    } else {
        ESP_LOGW("MUTEX", "Unlock attempted without lock");
    }
#endif
}

bool CN105Climate::is_wanted_settings_locked() const {
#ifdef USE_ESP32
    // Check if mutex would block (not perfect, but best we can do)
    return true;  // Assume locked if we can't check
#else
    return this->settings_lock_count_ > 0;
#endif
}
```

**FIX 4: Verwendung überall wo wantedSettings geändert wird**:
```cpp
// In climateControls.cpp
void CN105Climate::controlDelegate(const esphome::climate::ClimateCall& call) {
    ESP_LOGD("control", "espHome control() interface method called...");
    bool updated = false;

    // Protect settings access
    SettingsGuard guard(this);
    
    updated = this->processModeChange(call) || updated;
    updated = this->processTemperatureChange(call) || updated;
    updated = this->processFanChange(call) || updated;
    updated = this->processSwingChange(call) || updated;

    this->finalizeControlIfUpdated(updated);
    // Guard destructor automatically unlocks
}

// Und in andere Funktionen wo wantedSettings modified:
void CN105Climate::sendWantedSettings() {
    SettingsGuard guard(this);
    
    if (this->wantedSettings.hasBeenSent) {
        return;
    }
    
    // ... rest of function
    this->wantedSettings.hasBeenSent = true;
}
```

---

### FIX 4: Unchecked memcpy – Bounds Checking
**Datei**: `components/cn105/hp_writings.cpp` (Zeilen 16, 114)

**Neu in cn105_protocol.h**:
```cpp
/// Safe memcpy with bounds checking
/// Returns true if copy succeeded, false if would overflow
inline bool safe_memcpy(uint8_t* dest, size_t dest_size,
                        const uint8_t* src, size_t src_size) {
    if (!dest || !src) {
        return false;
    }
    if (src_size > dest_size) {
        ESP_LOGE("MEMCPY", "Buffer would overflow: %u > %u", src_size, dest_size);
        return false;
    }
    memcpy(dest, src, src_size);
    return true;
}

/// Specialized for arrays
template<size_t N>
inline bool safe_memcpy_array(uint8_t (&dest)[N],
                               const uint8_t* src, size_t src_size) {
    return safe_memcpy(dest, N, src, src_size);
}
```

**In hp_writings.cpp – Update Zeile 16**:
```cpp
// VORHER:
memcpy(packet, CONNECT, CONNECT_LEN);

// NACHHER:
if (!safe_memcpy_array(packet, CONNECT, CONNECT_LEN)) {
    ESP_LOGE(LOG_CONN_TAG, "Failed to copy CONNECT packet");
    return;
}
```

**In hp_writings.cpp – Update Zeile 114**:
```cpp
// VORHER:
memcpy(this->pending_packet_, packet, static_cast<size_t>(length));

// NACHHER:
if (!safe_memcpy(this->pending_packet_, sizeof(this->pending_packet_), 
                 packet, static_cast<size_t>(length))) {
    ESP_LOGE(TAG, "Failed to save pending packet");
    return;
}
```

---

## ⚠️ PERFORMANCE PROBLEME

### FIX 5: Verbose Logging blockiert UART
**Datei**: `components/cn105/hp_readings.cpp` (Zeilen 13-27)

**Aktuell (LANGSAM)**:
```cpp
bool CN105Climate::processInput(void) {
    bool processed = false;
    while (this->get_hw_serial_()->available()) {
        processed = true;
        uint8_t inputData;
        if (this->get_hw_serial_()->read_byte(&inputData)) {
            ESP_LOGV("Decoder", "--> %02X", inputData);  // JEDES BYTE!
            this->parser_.feed(inputData);
            if (this->parser_.frame_complete()) {
                this->processDataPacket();
                this->parser_.reset();
            }
        }
    }
    return processed;
}
```

**FIX (SCHNELL)**:
```cpp
bool CN105Climate::processInput(void) {
    bool processed = false;
    
#ifdef ENABLE_VERBOSE_UART_LOGGING
    // For detailed debugging: log each byte (WARNING: slow!)
    std::string frame_hex;
#endif
    
    while (this->get_hw_serial_()->available()) {
        processed = true;
        uint8_t inputData;
        if (this->get_hw_serial_()->read_byte(&inputData)) {
            // Only log at VERY_VERBOSE level to minimize performance impact
            ESP_LOGVV("Decoder", "--> %02X", inputData);
            
#ifdef ENABLE_VERBOSE_UART_LOGGING
            if (frame_hex.length() < 60) {  // Limit string size
                char hex_byte[4];
                snprintf(hex_byte, sizeof(hex_byte), "%02X ", inputData);
                frame_hex += hex_byte;
            }
#endif
            
            this->parser_.feed(inputData);
            if (this->parser_.frame_complete()) {
#ifdef ENABLE_VERBOSE_UART_LOGGING
                if (!frame_hex.empty()) {
                    ESP_LOGVV("Decoder", "Frame: %s", frame_hex.c_str());
                    frame_hex.clear();
                }
#endif
                this->processDataPacket();
                this->parser_.reset();
            }
        }
    }
    return processed;
}
```

**In Globals.h oder CMakeLists.txt – Conditional Logging**:
```cpp
// CMakeLists.txt
# option(ENABLE_VERBOSE_UART_LOGGING "Enable verbose UART byte logging (slow!)" OFF)
# if(ENABLE_VERBOSE_UART_LOGGING)
#     add_compile_definitions(ENABLE_VERBOSE_UART_LOGGING)
# endif()

// ODER in Globals.h (default OFF):
#ifndef ENABLE_VERBOSE_UART_LOGGING
#define ENABLE_VERBOSE_UART_LOGGING 0
#endif
```

---

### FIX 6: Checksum-Doppelberechnung – Caching
**Datei**: `components/cn105/frame_parser.h`

**Aktuell (INEFFIZIENT)**:
```cpp
bool checksum_valid() const {
    if (!frame_complete_) return false;
    uint8_t computed = checksum(buffer_, data_length_ + 5);  // Calculated here
    return computed == checksum_byte_;
}
```

**FIX (CACHED)**:
```cpp
class FrameParser {
private:
    uint8_t buffer_[MAX_DATA_BYTES]{};
    bool found_start_ = false;
    bool frame_complete_ = false;
    int bytes_read_ = 0;
    int data_length_ = -1;
    uint8_t command_ = 0;
    uint8_t checksum_byte_ = 0;
    bool checksum_valid_ = false;  // NEW: Cached result
    
public:
    void feed(uint8_t byte) {
        if (!found_start_) {
            if (byte == 0xFC) {
                found_start_ = true;
                bytes_read_ = 0;
                buffer_[bytes_read_++] = byte;
            }
            return;
        }

        if (bytes_read_ >= MAX_DATA_BYTES) {
            reset();
            return;
        }

        buffer_[bytes_read_] = byte;

        if (bytes_read_ == 4) {
            data_length_ = byte;
            command_ = buffer_[1];

            if ((data_length_ + 6) > MAX_DATA_BYTES) {
                reset();
                return;
            }
        }

        if (data_length_ >= 0 && bytes_read_ == data_length_ + 5) {
            checksum_byte_ = byte;
            frame_complete_ = true;
            
            // Calculate checksum ONCE when frame completes
            uint8_t computed = checksum(buffer_, data_length_ + 5);
            checksum_valid_ = (computed == checksum_byte_);
        } else {
            bytes_read_++;
        }
    }

    void reset() {
        found_start_ = false;
        frame_complete_ = false;
        bytes_read_ = 0;
        data_length_ = -1;
        command_ = 0;
        checksum_byte_ = 0;
        checksum_valid_ = false;  // Reset cache
    }

    bool checksum_valid() const { 
        return frame_complete_ && checksum_valid_;  // Just return cached value
    }
};
```

---

### FIX 7: Lookup Caching für häufige Werte
**Datei**: `components/cn105/hp_readings.cpp` (Zeilen 80-130)

**Neu in cn105.h oder separate Datei**:
```cpp
namespace cn105_protocol {

class LookupCache {
private:
    std::unordered_map<uint8_t, const char*> stage_cache_;
    std::unordered_map<uint8_t, const char*> mode_cache_;
    std::unordered_map<uint8_t, const char*> fan_cache_;
    bool initialized_ = false;
    
public:
    void initialize() {
        if (initialized_) return;
        
        // Pre-populate with known values
        for (int i = 0; i < STAGE_COUNT; i++) {
            stage_cache_[i] = STAGE[i];
        }
        
        for (int i = 0; i < MODE_COUNT; i++) {
            mode_cache_[i] = MODE[i];
        }
        
        for (int i = 0; i < FAN_SPEED_COUNT; i++) {
            fan_cache_[i] = FAN_SPEED[i];
        }
        
        initialized_ = true;
    }
    
    const char* get_stage(uint8_t byte, const char* default_val = "Unknown") {
        auto it = stage_cache_.find(byte);
        return it != stage_cache_.end() ? it->second : default_val;
    }
    
    const char* get_mode(uint8_t byte, const char* default_val = "Unknown") {
        auto it = mode_cache_.find(byte);
        return it != mode_cache_.end() ? it->second : default_val;
    }
    
    const char* get_fan(uint8_t byte, const char* default_val = "Unknown") {
        auto it = fan_cache_.find(byte);
        return it != fan_cache_.end() ? it->second : default_val;
    }
};

} // namespace cn105_protocol
```

**In cn105.h – Member Variable**:
```cpp
private:
    cn105_protocol::LookupCache lookup_cache_;
```

**In cn105.cpp – setup()**:
```cpp
void CN105Climate::setup() {
    lookup_cache_.initialize();
    // ... rest of setup
}
```

**In hp_readings.cpp – Verwendung**:
```cpp
void CN105Climate::getPowerFromResponsePacket() {
    ESP_LOGD("Decoder", "[0x09 is sub modes]");

    heatpumpSettings receivedSettings{};

    // Use cache for fast lookups (O(1) statt O(n))
    receivedSettings.stage = lookup_cache_.get_stage(data[4], this->currentSettings.stage);
    receivedSettings.sub_mode = lookup_cache_.get_mode(data[3], this->currentSettings.sub_mode);
    // ... rest
}
```

---

### FIX 8: String-Verkettung in Schleife
**Datei**: `components/cn105/utils.cpp` (Zeilen 460-472)

**Aktuell (INEFFIZIENT)**:
```cpp
char buffer[16];
std::string output;
for (/* ... */) {
    snprintf(buffer, sizeof(buffer), " %d:%d", code, value);
    output += buffer;  // String copies!
}
```

**FIX (EFFIZIENT)**:
```cpp
std::ostringstream oss;
for (/* ... */) {
    oss << " " << code << ":" << value;
}
std::string output = oss.str();  // Single allocation
```

---

## 🟡 FUNKTIONALE VERBESSERUNGEN

### FIX 9: Exponential Backoff für Reconnect
**Neu in cn105.h**:
```cpp
class ReconnectManager {
private:
    static constexpr uint32_t MIN_BACKOFF_MS = 1000;      // 1 second
    static constexpr uint32_t MAX_BACKOFF_MS = 60000;     // 60 seconds
    static constexpr uint8_t MAX_RETRY_COUNT = 10;
    
    uint32_t last_retry_time_ms_ = 0;
    uint8_t retry_count_ = 0;
    
public:
    void reset() {
        retry_count_ = 0;
        last_retry_time_ms_ = 0;
    }
    
    bool should_retry_now(uint32_t current_time_ms) {
        if (retry_count_ >= MAX_RETRY_COUNT) {
            ESP_LOGW("RECONNECT", "Max retries (%u) reached", MAX_RETRY_COUNT);
            return false;
        }
        
        uint32_t backoff_ms = get_backoff_ms();
        if (current_time_ms - last_retry_time_ms >= backoff_ms) {
            last_retry_time_ms = current_time_ms;
            retry_count_++;
            return true;
        }
        return false;
    }
    
    uint32_t get_backoff_ms() const {
        // Exponential backoff: 1s, 2s, 4s, 8s, 16s, 32s, 60s, ...
        uint32_t ms = MIN_BACKOFF_MS << retry_count_;
        return std::min(ms, MAX_BACKOFF_MS);
    }
    
    uint8_t get_retry_count() const {
        return retry_count_;
    }
};
```

**In cn105.h – Member**:
```cpp
private:
    ReconnectManager reconnect_manager_;
```

**In componentEntries.cpp – Verwendung**:
```cpp
case DriverState::DISCONNECTED: {
    if (this->reconnect_manager_.should_retry_now(CUSTOM_MILLIS)) {
        uint32_t next_backoff = this->reconnect_manager_.get_backoff_ms();
        ESP_LOGI(LOG_CONN_TAG, "Reconnect attempt #%u (next backoff: %u ms)",
                 this->reconnect_manager_.get_retry_count(),
                 next_backoff);
        this->setupUART();
        this->sendFirstConnectionPacket();
    }
    return;
}

case DriverState::CONNECTED: {
    this->reconnect_manager_.reset();  // Reset on successful connection
    // ... existing code
}
```

---

### FIX 10: Konfigurierbare Timeouts
**In cn105.h – Member Variables**:
```cpp
private:
    struct TimeoutConfig {
        uint32_t bootstrap_ms = 120000;      // 120 seconds
        uint32_t connect_response_ms = 3000;  // 3 seconds
        uint32_t info_response_ms = 800;      // 800 ms
        uint32_t remote_temp_ms = 300000;     // 5 minutes
    } timeout_config_;
    
public:
    void set_bootstrap_timeout(uint32_t timeout_ms) { 
        timeout_config_.bootstrap_ms = timeout_ms; 
    }
    
    void set_connect_response_timeout(uint32_t timeout_ms) { 
        timeout_config_.connect_response_ms = timeout_ms; 
    }
    
    void set_info_response_timeout(uint32_t timeout_ms) { 
        timeout_config_.info_response_ms = timeout_ms; 
    }
    
    void set_remote_temp_timeout(uint32_t timeout_ms) {
        timeout_config_.remote_temp_ms = timeout_ms;
    }
    
    const TimeoutConfig& get_timeout_config() const {
        return timeout_config_;
    }
```

**In componentEntries.cpp – Verwendung**:
```cpp
case DriverState::BOOT: {
    this->set_timeout("cn105_bootstrap_timeout", 
                      timeout_config_.bootstrap_ms,  // Use config
                      [this]() {
        if (state_ >= DriverState::CONNECTING) return;
        ESP_LOGW(LOG_CONN_TAG, "Bootstrap connection: timeout %u ms, starting CN105 anyway",
                 timeout_config_.bootstrap_ms);
        this->setupUART();
        this->sendFirstConnectionPacket();
    });
    // ...
}
```

**In Python Config (__init__.py)**:
```python
# Make it configurable via YAML
CONF_BOOTSTRAP_TIMEOUT = "bootstrap_timeout"
CONF_CONNECT_TIMEOUT = "connect_timeout"
CONF_INFO_TIMEOUT = "info_timeout"
CONF_REMOTE_TEMP_TIMEOUT = "remote_temp_timeout"

# In schema:
cv.Optional(CONF_BOOTSTRAP_TIMEOUT, default=120000): cv.int_range(min=1000, max=300000),
cv.Optional(CONF_CONNECT_TIMEOUT, default=3000): cv.int_range(min=500, max=30000),
cv.Optional(CONF_INFO_TIMEOUT, default=800): cv.int_range(min=100, max=5000),
cv.Optional(CONF_REMOTE_TEMP_TIMEOUT, default=300000): cv.int_range(min=10000, max=3600000),

# In setup:
cg.add(var.set_bootstrap_timeout(config[CONF_BOOTSTRAP_TIMEOUT]))
cg.add(var.set_connect_response_timeout(config[CONF_CONNECT_TIMEOUT]))
cg.add(var.set_info_response_timeout(config[CONF_INFO_TIMEOUT]))
cg.add(var.set_remote_temp_timeout(config[CONF_REMOTE_TEMP_TIMEOUT]))
```

---

### FIX 11: Remote Temperature Watchdog realistisch
**In cn105.cpp**:
```cpp
CN105Climate::CN105Climate(uart::UARTComponent* uart) :
    // ... existing code ...
{
    // Don't set to UINT32_MAX anymore
    this->remote_temp_timeout_ = 300000;  // 5 minutes default
    
    // ... rest
}

void CN105Climate::setup() {
    // ...
    
    // If remote temp source exists but no timeout set, use default
    if (remote_temp_source_ != nullptr && remote_temp_timeout_ == UINT32_MAX) {
        this->remote_temp_timeout_ = 300000;  // 5 minutes
        ESP_LOGI(LOG_REMOTE_TEMP, "Remote temp timeout set to default: 5 minutes");
    }
    
    // ...
}
```

**Add Setter Validation**:
```cpp
void CN105Climate::set_remote_temp_timeout(uint32_t timeout) {
    if (timeout < 10000 || timeout > 3600000) {
        ESP_LOGW(LOG_REMOTE_TEMP, "Invalid timeout %u ms, using default 5 min", timeout);
        this->remote_temp_timeout_ = 300000;
    } else {
        this->remote_temp_timeout_ = timeout;
        ESP_LOGI(LOG_REMOTE_TEMP, "Remote temp timeout: %u ms", timeout);
    }
}
```

---

### FIX 12: Zyklusstatistiken für Diagnostik
**Neu in cn105.h**:
```cpp
struct CycleStatistics {
    uint32_t total_cycles = 0;
    uint32_t successful_cycles = 0;
    uint32_t failed_cycles = 0;
    uint32_t timed_out_cycles = 0;
    
    uint32_t min_cycle_ms = UINT32_MAX;
    uint32_t max_cycle_ms = 0;
    uint32_t avg_cycle_ms = 0;
    
    uint32_t uptime_ms = 0;
    uint32_t last_update_ms = 0;
    
    void record_cycle(uint32_t duration_ms, bool success) {
        total_cycles++;
        
        if (success) {
            successful_cycles++;
        } else {
            failed_cycles++;
        }
        
        // Track min/max/avg
        min_cycle_ms = std::min(min_cycle_ms, duration_ms);
        max_cycle_ms = std::max(max_cycle_ms, duration_ms);
        
        // Moving average
        if (total_cycles == 1) {
            avg_cycle_ms = duration_ms;
        } else {
            avg_cycle_ms = (avg_cycle_ms * (total_cycles - 1) + duration_ms) / total_cycles;
        }
        
        last_update_ms = CUSTOM_MILLIS;
    }
    
    void record_timeout() {
        timed_out_cycles++;
        failed_cycles++;
    }
    
    float get_success_rate() const {
        if (total_cycles == 0) return 0.0f;
        return (float)successful_cycles / (float)total_cycles * 100.0f;
    }
    
    std::string to_string() const {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Cycles: %u total, %u OK, %u fail, %u timeout | "
                 "Duration: min=%u, max=%u, avg=%u ms | "
                 "Success rate: %.1f%%",
                 total_cycles, successful_cycles, failed_cycles, timed_out_cycles,
                 min_cycle_ms, max_cycle_ms, avg_cycle_ms,
                 get_success_rate());
        return buf;
    }
};
```

**In cn105.h – Member**:
```cpp
private:
    CycleStatistics cycle_stats_;
    
public:
    const CycleStatistics& get_cycle_statistics() const {
        return cycle_stats_;
    }
    
    std::string get_diagnostics_string() const {
        return cycle_stats_.to_string();
    }
```

**In cn105.cpp – Recording**:
```cpp
void CN105Climate::terminateCycle() {
    // ... existing code ...
    
    uint32_t cycle_duration = CUSTOM_MILLIS - this->loopCycle.lastCycleStartMs;
    bool success = /* determine if cycle was successful */;
    cycle_stats_.record_cycle(cycle_duration, success);
    
    if (total_cycles % 100 == 0) {  // Log every 100 cycles
        ESP_LOGI(TAG, "%s", cycle_stats_.to_string().c_str());
    }
}
```

---

## 📊 CODE QUALITY ISSUES

### FIX 13: Riesiger Header – Builder Pattern
**Neu: components/cn105/cn105_builder.h**:
```cpp
#pragma once
#include "cn105.h"

namespace esphome {

class CN105Builder {
private:
    CN105Climate& climate_;
    
public:
    explicit CN105Builder(CN105Climate& climate) : climate_(climate) {}
    
    // Sensors
    CN105Builder& with_compressor_frequency(sensor::Sensor* s) {
        climate_.set_compressor_frequency_sensor(s);
        return *this;
    }
    
    CN105Builder& with_target_humidity(sensor::Sensor* s) {
        climate_.set_target_humidity_sensor(s);
        return *this;
    }
    
    CN105Builder& with_input_power(sensor::Sensor* s) {
        climate_.set_input_power_sensor(s);
        return *this;
    }
    
    CN105Builder& with_kwh_sensor(sensor::Sensor* s) {
        climate_.set_kwh_sensor(s);
        return *this;
    }
    
    CN105Builder& with_runtime_hours(sensor::Sensor* s) {
        climate_.set_runtime_hours_sensor(s);
        return *this;
    }
    
    CN105Builder& with_outside_air_temp(sensor::Sensor* s) {
        climate_.set_outside_air_temperature_sensor(s);
        return *this;
    }
    
    // Binary Sensors
    CN105Builder& with_isee_sensor(binary_sensor::BinarySensor* s) {
        climate_.set_isee_sensor(s);
        return *this;
    }
    
    CN105Builder& with_remote_temp_control(binary_sensor::BinarySensor* s) {
        climate_.set_remote_temperature_control_sensor(s);
        return *this;
    }
    
    // Text Sensors
    CN105Builder& with_stage_sensor(text_sensor::TextSensor* s) {
        climate_.set_stage_sensor(s);
        return *this;
    }
    
    CN105Builder& with_functions_sensor(text_sensor::TextSensor* s) {
        climate_.set_functions_sensor(s);
        return *this;
    }
    
    CN105Builder& with_sub_mode_sensor(text_sensor::TextSensor* s) {
        climate_.set_sub_mode_sensor(s);
        return *this;
    }
    
    CN105Builder& with_error_code_sensor(text_sensor::TextSensor* s) {
        climate_.set_error_code_sensor(s);
        return *this;
    }
    
    // Selects
    CN105Builder& with_vertical_vane(VaneOrientationSelect* s) {
        climate_.set_vertical_vane_select(s);
        return *this;
    }
    
    CN105Builder& with_horizontal_vane(VaneOrientationSelect* s) {
        climate_.set_horizontal_vane_select(s);
        return *this;
    }
    
    // Timeouts & Intervals
    CN105Builder& with_update_interval(uint32_t ms) {
        climate_.set_update_interval(ms);
        return *this;
    }
    
    CN105Builder& with_remote_temp_timeout(uint32_t ms) {
        climate_.set_remote_temp_timeout(ms);
        return *this;
    }
    
    CN105Builder& with_bootstrap_delay(uint32_t ms) {
        climate_.set_connection_bootstrap_delay(ms);
        return *this;
    }
    
    // Finalize
    CN105Climate& build() {
        return climate_;
    }
};

} // namespace esphome
```

**Verwendung in __init__.py**:
```python
# Vorher: viele einzelne cg.add() calls
# Nachher:
cg.add(
    CN105Builder(var)
        .with_compressor_frequency(compressor_sensor)
        .with_target_humidity(humidity_sensor)
        .with_kwh_sensor(kwh_sensor)
        .with_bootstrap_delay(config[CONF_BOOTSTRAP_DELAY])
        .build()
)
```

---

### FIX 14: Fehlerbehandlung standardisieren
**Neu in cn105_types.h**:
```cpp
namespace cn105_protocol {

enum class CN105Error : uint8_t {
    NONE = 0,
    UART_NOT_READY = 1,
    CHECKSUM_FAIL = 2,
    TIMEOUT = 3,
    BUFFER_OVERFLOW = 4,
    INVALID_STATE = 5,
    FRAME_TOO_LONG = 6,
    INVALID_COMMAND = 7,
    SETTINGS_LOCKED = 8,
};

struct Result {
    CN105Error error = CN105Error::NONE;
    const char* message = "";
    
    bool is_ok() const { 
        return error == CN105Error::NONE; 
    }
    
    const char* to_string() const {
        switch (error) {
            case CN105Error::NONE: return "OK";
            case CN105Error::UART_NOT_READY: return "UART not ready";
            case CN105Error::CHECKSUM_FAIL: return "Checksum mismatch";
            case CN105Error::TIMEOUT: return "Operation timeout";
            case CN105Error::BUFFER_OVERFLOW: return "Buffer overflow";
            case CN105Error::INVALID_STATE: return "Invalid state";
            case CN105Error::FRAME_TOO_LONG: return "Frame too long";
            case CN105Error::INVALID_COMMAND: return "Invalid command";
            case CN105Error::SETTINGS_LOCKED: return "Settings locked";
            default: return "Unknown error";
        }
    }
};

} // namespace cn105_protocol
```

**Verwendung**:
```cpp
cn105_protocol::Result result = this->processDataPacket();
if (!result.is_ok()) {
    ESP_LOGE(TAG, "Error: %s", result.to_string());
    return;
}
```

---

### FIX 15: Große Funktionen aufteilen
**Beispiel: climateControls.cpp `processTemperatureChange`**

**Vorher (100+ Zeilen in einer Funktion)**:
```cpp
bool CN105Climate::processTemperatureChange(const esphome::climate::ClimateCall& call) {
    // 100+ lines of mixed logic
}
```

**Nachher (aufgeteilt)**:
```cpp
// Helper functions - each ~20 lines
bool CN105Climate::extractRequestedTemperatures(const esphome::climate::ClimateCall& call,
                                                 float& out_low, float& out_high) {
    bool has_low = call.get_target_temperature_low().has_value();
    bool has_high = call.get_target_temperature_high().has_value();
    bool has_single = call.get_target_temperature().has_value();
    
    if (has_low) out_low = *call.get_target_temperature_low();
    if (has_high) out_high = *call.get_target_temperature_high();
    if (has_single && !has_low && !has_high) out_low = out_high = *call.get_target_temperature();
    
    return (has_low || has_high || has_single);
}

bool CN105Climate::validateTemperatureRange(float low, float high) {
    if (low > high) {
        ESP_LOGW(TAG, "Invalid range: low=%.1f > high=%.1f", low, high);
        return false;
    }
    return true;
}

void CN105Climate::applyDualSetpointTemperatures(float low, float high) {
    setTargetTemperatureLow(low);
    setTargetTemperatureHigh(high);
}

bool CN105Climate::processTemperatureChange(const esphome::climate::ClimateCall& call) {
    float temp_low = getCurrentTemperature();
    float temp_high = getCurrentTemperature();
    
    if (!extractRequestedTemperatures(call, temp_low, temp_high)) {
        return false;
    }
    
    if (!validateTemperatureRange(temp_low, temp_high)) {
        return false;
    }
    
    if (this->traits_.supports_two_point_target_temperature()) {
        applyDualSetpointTemperatures(temp_low, temp_high);
    } else {
        setTargetTemperature(temp_low);
    }
    
    return true;
}
```

---

### FIX 16: Manual Arrays → std::array
**In frame_parser.h**:
```cpp
// VORHER:
uint8_t buffer_[MAX_DATA_BYTES]{};

// NACHHER:
std::array<uint8_t, MAX_DATA_BYTES> buffer_{};

// Usage bleibt gleich:
buffer_[0] = 0xFC;  // Works with std::array
```

**Benefit**:
- Automatic bounds checking available
- Better API (`.size()`, `.data()`, range-based loops)
- Compatible with STL algorithms

---

## 📋 ZUSAMMENFASSUNG – IMPLEMENTATION PRIORITÄT

| Fix # | Titel | Datei(en) | Aufwand | Kritikalität |
|-------|-------|-----------|---------|------------|
| 1 | Remove const_cast | hp_readings.cpp | 30 min | 🔴 KRITISCH |
| 2 | String statt char[256] | heatpumpFunctions.cpp | 30 min | 🔴 KRITISCH |
| 3 | Unified Mutex | cn105.h/cpp | 2 hours | 🔴 KRITISCH |
| 4 | Safe memcpy | hp_writings.cpp | 1 hour | 🔴 KRITISCH |
| 5 | Reduce Verbose Logging | hp_readings.cpp | 30 min | 🟠 WICHTIG |
| 6 | Cache Checksum | frame_parser.h | 30 min | 🟠 WICHTIG |
| 7 | Lookup Cache | hp_readings.cpp | 1 hour | 🟠 WICHTIG |
| 8 | Efficient String Concat | utils.cpp | 30 min | 🟠 WICHTIG |
| 9 | Exponential Backoff | cn105.h/cpp | 1.5 hours | 🟡 SPÄTER |
| 10 | Configurable Timeouts | cn105.h/__init__.py | 2 hours | 🟡 SPÄTER |
| 11 | Remote Temp Timeout | cn105.cpp | 30 min | 🟡 SPÄTER |
| 12 | Cycle Statistics | cn105.h/cpp | 1 hour | 🟡 SPÄTER |
| 13 | Builder Pattern | cn105_builder.h | 1.5 hours | 🟡 SPÄTER |
| 14 | Error Handling | cn105_types.h | 1 hour | 🟡 SPÄTER |
| 15 | Split Functions | climateControls.cpp | 2 hours | 🟡 SPÄTER |
| 16 | Use std::array | frame_parser.h | 30 min | 🟡 SPÄTER |

**Empfohlene Reihenfolge für Implementation:**
1. Fixes 1-4 (Critical Security)
2. Fixes 5-8 (Performance)
3. Fixes 9-12 (Features)
4. Fixes 13-16 (Code Quality)

