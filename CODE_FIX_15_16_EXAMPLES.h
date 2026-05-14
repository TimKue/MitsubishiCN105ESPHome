// FIX 15: Example of splitting large functions into smaller ones
// This shows how to refactor processTemperatureChange to be more testable

// Before: 100+ lines in one function
// After: Split into focused helper functions (~20 lines each)

// Example helpers that would replace the monolithic processTemperatureChange:

/*

// Helper 1: Extract requested temperature values from ClimateCall
bool CN105Climate::extractRequestedTemperatures(
    const esphome::climate::ClimateCall& call,
    float& out_low, float& out_high) {
    bool has_low = call.get_target_temperature_low().has_value();
    bool has_high = call.get_target_temperature_high().has_value();
    bool has_single = call.get_target_temperature().has_value();
    
    if (has_low) out_low = *call.get_target_temperature_low();
    if (has_high) out_high = *call.get_target_temperature_high();
    if (has_single && !has_low && !has_high) {
        out_low = out_high = *call.get_target_temperature();
    }
    
    return (has_low || has_high || has_single);
}

// Helper 2: Validate temperature range
bool CN105Climate::validateTemperatureRange(float low, float high) {
    if (low > high) {
        ESP_LOGW(TAG, "Invalid range: low=%.1f > high=%.1f", low, high);
        return false;
    }
    if (low < ESPMHP_MIN_TEMPERATURE || high > ESPMHP_MAX_TEMPERATURE) {
        ESP_LOGW(TAG, "Temperature out of range: [%.1f, %.1f]", low, high);
        return false;
    }
    return true;
}

// Helper 3: Apply temperatures based on mode
void CN105Climate::applyTemperatureChange(float temp_low, float temp_high) {
    if (this->traits_.supports_two_point_target_temperature()) {
        setTargetTemperatureLow(temp_low);
        setTargetTemperatureHigh(temp_high);
    } else {
        setTargetTemperature(temp_low);
    }
}

// Refactored main function: Now just orchestrates the helpers
bool CN105Climate::processTemperatureChange(const esphome::climate::ClimateCall& call) {
    float temp_low = getCurrentTemperature();
    float temp_high = getCurrentTemperature();
    
    // Extract
    if (!extractRequestedTemperatures(call, temp_low, temp_high)) {
        return false;
    }
    
    // Validate
    if (!validateTemperatureRange(temp_low, temp_high)) {
        return false;
    }
    
    // Apply
    applyTemperatureChange(temp_low, temp_high);
    return true;
}

*/

// FIX 16: Use std::array instead of C-style arrays
// Example transformation in frame_parser.h:

/*

// Before: Raw C array
uint8_t buffer_[MAX_DATA_BYTES]{};

// After: std::array with bounds checking
std::array<uint8_t, MAX_DATA_BYTES> buffer_;

// Usage: Syntax remains the same, but with better safety:
buffer_[0] = 0xFC;           // Still works
auto data = buffer_.data();   // Get pointer if needed
auto size = buffer_.size();   // Get size safely
for (auto byte : buffer_) {}  // Range-based loop available

*/
