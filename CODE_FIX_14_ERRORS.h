// This file was generated as part of the code fixes.
// FIX 14: Error handling enum for structured error codes
// To be integrated into cn105_types.h

#pragma once

namespace cn105_protocol {

/// Enumeration of all possible error codes
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

/// Standard result type for operations
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
