// FIX 13: Builder Pattern for cleaner API
// This file demonstrates the builder pattern for CN105Climate configuration
// Example usage in Python/ESPHome YAML generation:

#pragma once

#include "cn105.h"

namespace esphome {

/// Builder pattern for CN105Climate component
/// Enables fluent API for configuration
class CN105Builder {
private:
    CN105Climate& climate_;
    
public:
    explicit CN105Builder(CN105Climate& climate) : climate_(climate) {}
    
    // === Sensors ===
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
    
    // === Binary Sensors ===
    CN105Builder& with_isee_sensor(binary_sensor::BinarySensor* s) {
        climate_.set_isee_sensor(s);
        return *this;
    }
    
    CN105Builder& with_remote_temp_control(binary_sensor::BinarySensor* s) {
        climate_.set_remote_temperature_control_sensor(s);
        return *this;
    }
    
    // === Text Sensors ===
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
    
    // === Selects ===
    CN105Builder& with_vertical_vane(VaneOrientationSelect* s) {
        climate_.set_vertical_vane_select(s);
        return *this;
    }
    
    CN105Builder& with_horizontal_vane(VaneOrientationSelect* s) {
        climate_.set_horizontal_vane_select(s);
        return *this;
    }
    
    // === Timeouts & Intervals ===
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
    
    CN105Builder& with_connect_timeout(uint32_t ms) {
        climate_.set_connect_response_timeout(ms);
        return *this;
    }
    
    CN105Builder& with_info_timeout(uint32_t ms) {
        climate_.set_info_response_timeout(ms);
        return *this;
    }
    
    // === Finalize ===
    CN105Climate& build() {
        return climate_;
    }
};

} // namespace esphome
