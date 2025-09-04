// Scalar value representation for GW-BASIC variables.
#pragma once

#include <cstdint>
#include <stdexcept>
#include "StringTypes.hpp"

namespace gwbasic {

enum class ScalarType : uint8_t { Int16, Single, Double, String };

struct Value {
    ScalarType type{ScalarType::Single};
    // Storage
    int16_t i{0};
    float f{0.0f};
    double d{0.0};
    StrDesc s{};

    static Value makeInt(int16_t v) {
        Value x; x.type = ScalarType::Int16; x.i = v; return x;
    }
    static Value makeSingle(float v) {
        Value x; x.type = ScalarType::Single; x.f = v; return x;
    }
    static Value makeDouble(double v) {
        Value x; x.type = ScalarType::Double; x.d = v; return x;
    }
    static Value makeString(const StrDesc& sd) {
        Value x; x.type = ScalarType::String; x.s = sd; return x;
    }
};

inline ScalarType typeFromSuffix(char suffix) {
    switch (suffix) {
        case '%': return ScalarType::Int16;
        case '!': return ScalarType::Single;
        case '#': return ScalarType::Double;
        case '$': return ScalarType::String;
        default:  return ScalarType::Single; // default; caller may override by DEFTBL
    }
}

} // namespace gwbasic
