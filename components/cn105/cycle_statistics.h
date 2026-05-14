#pragma once

#include <cstdint>
#include <cstdio>
#include <algorithm>
#include "Globals.h"

namespace esphome {

/// FIX 12: Cycle statistics for diagnostics and monitoring
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
    
    /// Record a completed cycle with its duration
    void record_cycle(uint32_t duration_ms, bool success) {
        total_cycles++;
        
        if (success) {
            successful_cycles++;
        } else {
            failed_cycles++;
        }
        
        // Track min/max
        min_cycle_ms = std::min(min_cycle_ms, duration_ms);
        max_cycle_ms = std::max(max_cycle_ms, duration_ms);
        
        // Moving average
        if (total_cycles == 1) {
            avg_cycle_ms = duration_ms;
        } else {
            avg_cycle_ms = (avg_cycle_ms * (total_cycles - 1) + duration_ms) / total_cycles;
        }
        
        last_update_ms = esphome::millis();
    }
    
    /// Record a timeout
    void record_timeout() {
        timed_out_cycles++;
        failed_cycles++;
        total_cycles++;
    }
    
    /// Calculate success rate as percentage
    float get_success_rate() const {
        if (total_cycles == 0) return 0.0f;
        return (float)successful_cycles / (float)total_cycles * 100.0f;
    }
    
    /// Convert statistics to human-readable string
    std::string to_string() const {
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "Cycles: %u total, %u OK, %u fail, %u timeout | "
                 "Duration: min=%u, max=%u, avg=%u ms | "
                 "Success rate: %.1f%%",
                 total_cycles, successful_cycles, failed_cycles, timed_out_cycles,
                 min_cycle_ms == UINT32_MAX ? 0 : min_cycle_ms, max_cycle_ms, avg_cycle_ms,
                 get_success_rate());
        return std::string(buf);
    }
};

} // namespace esphome
