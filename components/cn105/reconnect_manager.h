#pragma once

#include <cstdint>
#include <algorithm>
#include "Globals.h"

namespace esphome {

/// FIX 9: Reconnect manager with exponential backoff
class ReconnectManager {
private:
    static constexpr uint32_t MIN_BACKOFF_MS = 1000;      // 1 second
    static constexpr uint32_t MAX_BACKOFF_MS = 60000;     // 60 seconds
    static constexpr uint8_t MAX_RETRY_COUNT = 10;
    
    uint32_t last_retry_time_ms_ = 0;
    uint8_t retry_count_ = 0;
    
public:
    /// Reset backoff counter (call on successful connection)
    void reset() {
        retry_count_ = 0;
        last_retry_time_ms_ = 0;
    }
    
    /// Check if enough time has passed to retry
    /// Returns true if should retry now
    bool should_retry_now(uint32_t current_time_ms) {
        if (retry_count_ >= MAX_RETRY_COUNT) {
            ESP_LOGW("RECONNECT", "Max retries (%u) reached, giving up", MAX_RETRY_COUNT);
            return false;
        }
        
        uint32_t backoff_ms = get_backoff_ms();
        if (current_time_ms - last_retry_time_ms_ >= backoff_ms) {
            last_retry_time_ms_ = current_time_ms;
            retry_count_++;
            return true;
        }
        return false;
    }
    
    /// Get current backoff duration in milliseconds
    /// Exponential: 1s, 2s, 4s, 8s, 16s, 32s, 60s, ...
    uint32_t get_backoff_ms() const {
        uint32_t ms = MIN_BACKOFF_MS << retry_count_;
        return std::min(ms, MAX_BACKOFF_MS);
    }
    
    /// Get current retry count
    uint8_t get_retry_count() const {
        return retry_count_;
    }
    
    /// Get max retry count
    uint8_t get_max_retries() const {
        return MAX_RETRY_COUNT;
    }
};

} // namespace esphome
