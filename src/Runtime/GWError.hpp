#pragma once

#include <stdexcept>
#include <string>
#include <cstdint>

namespace gwbasic {

/**
 * GWError - Standard GW-BASIC runtime error
 * 
 * This exception type represents runtime errors that occur during
 * program execution, carrying GW-BASIC compatible error codes.
 */
class GWError : public std::runtime_error {
public:
    GWError(uint16_t errorCode, const std::string& message, uint16_t lineNumber = 0, size_t position = 0)
        : std::runtime_error(message), errorCode_(errorCode), lineNumber_(lineNumber), position_(position) {}
    
    uint16_t getErrorCode() const { return errorCode_; }
    uint16_t getLineNumber() const { return lineNumber_; }
    size_t getPosition() const { return position_; }
    
    void setLineNumber(uint16_t lineNumber) { lineNumber_ = lineNumber; }

private:
    uint16_t errorCode_;
    uint16_t lineNumber_;
    size_t position_;
};

// Common GW-BASIC error codes
namespace ErrorCodes {
    constexpr uint16_t NEXT_WITHOUT_FOR = 1;
    constexpr uint16_t SYNTAX_ERROR = 2;
    constexpr uint16_t RETURN_WITHOUT_GOSUB = 3;
    constexpr uint16_t OUT_OF_DATA = 4;
    constexpr uint16_t ILLEGAL_FUNCTION_CALL = 5;
    constexpr uint16_t OVERFLOW = 6;
    constexpr uint16_t OUT_OF_MEMORY = 7;
    constexpr uint16_t UNDEFINED_LINE_NUMBER = 8;
    constexpr uint16_t SUBSCRIPT_OUT_OF_RANGE = 9;
    constexpr uint16_t DUPLICATE_DEFINITION = 10;
    constexpr uint16_t DIVISION_BY_ZERO = 11;
    constexpr uint16_t ILLEGAL_DIRECT = 12;
    constexpr uint16_t TYPE_MISMATCH = 13;
    constexpr uint16_t OUT_OF_STRING_SPACE = 14;
    constexpr uint16_t STRING_TOO_LONG = 15;
    constexpr uint16_t STRING_FORMULA_TOO_COMPLEX = 16;
    constexpr uint16_t CANT_CONTINUE = 17;
    constexpr uint16_t UNDEFINED_USER_FUNCTION = 18;
    constexpr uint16_t NO_RESUME = 19;
    constexpr uint16_t RESUME_WITHOUT_ERROR = 20;
    constexpr uint16_t UNPRINTABLE_ERROR = 21;
    constexpr uint16_t MISSING_OPERAND = 22;
    constexpr uint16_t LINE_BUFFER_OVERFLOW = 23;
    constexpr uint16_t DEVICE_TIMEOUT = 24;
    constexpr uint16_t DEVICE_FAULT = 25;
    constexpr uint16_t FOR_WITHOUT_NEXT = 26;
    constexpr uint16_t OUT_OF_PAPER = 27;
    constexpr uint16_t DISK_FULL = 61;
    constexpr uint16_t FILE_NOT_FOUND = 53;
    constexpr uint16_t BAD_FILE_MODE = 54;
    constexpr uint16_t FILE_ALREADY_OPEN = 55;
    constexpr uint16_t FIELD_OVERFLOW = 50;
    constexpr uint16_t INTERNAL_ERROR = 51;
    constexpr uint16_t BAD_FILE_NUMBER = 52;
    constexpr uint16_t DISK_NOT_READY = 71;
    constexpr uint16_t DISK_MEDIA_ERROR = 72;
    constexpr uint16_t ADVANCED_FEATURE = 73;
    constexpr uint16_t RENAME_ACROSS_DISKS = 74;
    constexpr uint16_t PATH_FILE_ACCESS_ERROR = 75;
    constexpr uint16_t PATH_NOT_FOUND = 76;
}

} // namespace gwbasic