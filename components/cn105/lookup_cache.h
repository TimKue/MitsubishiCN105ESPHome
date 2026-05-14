#pragma once

#include <unordered_map>
#include "cn105_types.h"

namespace cn105_protocol {

/// Lookup cache for O(1) access to frequently used value maps
/// Replaces O(n) lookups with O(1) hash table access
class LookupCache {
private:
    std::unordered_map<uint8_t, const char*> stage_cache_;
    std::unordered_map<uint8_t, const char*> mode_cache_;
    std::unordered_map<uint8_t, const char*> fan_cache_;
    std::unordered_map<uint8_t, const char*> sub_mode_cache_;
    std::unordered_map<uint8_t, const char*> auto_sub_mode_cache_;
    bool initialized_ = false;
    
public:
    /// Initialize all caches with default values
    void initialize() {
        if (initialized_) return;
        
        // Pre-populate with known values (defined in cn105_types.h)
        for (int i = 0; i < 7; i++) {
            stage_cache_[STAGE[i]] = STAGE_MAP[i];
        }
        
        for (int i = 0; i < 5; i++) {
            mode_cache_[MODE[i]] = MODE_MAP[i];
        }
        
        for (int i = 0; i < 6; i++) {
            fan_cache_[FAN[i]] = FAN_MAP[i];
        }
        
        for (int i = 0; i < 6; i++) {
            sub_mode_cache_[SUB_MODE[i]] = SUB_MODE_MAP[i];
        }
        
        for (int i = 0; i < 7; i++) {
            auto_sub_mode_cache_[AUTO_SUB_MODE[i]] = AUTO_SUB_MODE_MAP[i];
        }
        
        initialized_ = true;
    }
    
    /// O(1) lookup for stage values
    const char* get_stage(uint8_t byte, const char* default_val = "Unknown") {
        auto it = stage_cache_.find(byte);
        return it != stage_cache_.end() ? it->second : default_val;
    }
    
    /// O(1) lookup for mode values
    const char* get_mode(uint8_t byte, const char* default_val = "Unknown") {
        auto it = mode_cache_.find(byte);
        return it != mode_cache_.end() ? it->second : default_val;
    }
    
    /// O(1) lookup for fan values
    const char* get_fan(uint8_t byte, const char* default_val = "Unknown") {
        auto it = fan_cache_.find(byte);
        return it != fan_cache_.end() ? it->second : default_val;
    }
    
    /// O(1) lookup for sub_mode values
    const char* get_sub_mode(uint8_t byte, const char* default_val = "Unknown") {
        auto it = sub_mode_cache_.find(byte);
        return it != sub_mode_cache_.end() ? it->second : default_val;
    }
    
    /// O(1) lookup for auto_sub_mode values
    const char* get_auto_sub_mode(uint8_t byte, const char* default_val = "Unknown") {
        auto it = auto_sub_mode_cache_.find(byte);
        return it != auto_sub_mode_cache_.end() ? it->second : default_val;
    }
};

} // namespace cn105_protocol
