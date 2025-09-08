#pragma once

#include <cstdint>
#include <array>
#include <string>

/**
 * Microsoft Binary Format (MBF) Implementation
 * 
 * This module provides conversion between IEEE 754 floating-point format
 * and Microsoft Binary Format (MBF) used by GW-BASIC.
 * 
 * MBF Format:
 * - Base-2 normalized format with bias 128 exponent
 * - No NaNs, Infinities, or subnormal numbers
 * - Sign stored in high bit of mantissa, not exponent
 * - Implicit leading 1 bit (not stored)
 * 
 * Single Precision (MBF32 - 4 bytes):
 * - Byte 0: Exponent (bias 128), 0 = zero
 * - Byte 1: Sign bit (0x80) + top 7 mantissa bits
 * - Byte 2: Next 8 mantissa bits  
 * - Byte 3: Lowest 8 mantissa bits
 * - Total: 23 stored mantissa bits + 1 implicit = 24 significant bits
 * 
 * Double Precision (MBF64 - 8 bytes):
 * - Byte 0: Exponent (bias 128), 0 = zero
 * - Byte 1: Sign bit (0x80) + top 7 mantissa bits
 * - Bytes 2-7: Remaining 48 mantissa bits
 * - Total: 55 stored mantissa bits + 1 implicit = 56 significant bits
 */

namespace MBF {

// MBF Constants
constexpr uint8_t MBF_EXPONENT_BIAS = 128;
constexpr uint8_t MBF_SIGN_MASK = 0x80;
constexpr uint8_t MBF_MANTISSA_MASK = 0x7F;

// IEEE 754 Constants  
constexpr uint8_t IEEE_SINGLE_EXPONENT_BIAS = 127;
constexpr uint16_t IEEE_DOUBLE_EXPONENT_BIAS = 1023;

/**
 * MBF32 - Microsoft Binary Format Single Precision (4 bytes)
 */
struct MBF32 {
    uint8_t exponent;    // Bias 128, 0 = zero
    uint8_t mantissa[3]; // mantissa[0] has sign bit + 7 mantissa bits
    
    MBF32() : exponent(0), mantissa{0, 0, 0} {}
    MBF32(uint8_t exp, uint8_t m0, uint8_t m1, uint8_t m2) 
        : exponent(exp), mantissa{m0, m1, m2} {}
    
    // Construct from byte array
    explicit MBF32(const uint8_t bytes[4]) 
        : exponent(bytes[0]), mantissa{bytes[1], bytes[2], bytes[3]} {}
    
    // Convert to byte array
    void toBytes(uint8_t bytes[4]) const {
        bytes[0] = exponent;
        bytes[1] = mantissa[0];
        bytes[2] = mantissa[1]; 
        bytes[3] = mantissa[2];
    }
    
    bool isZero() const { return exponent == 0; }
    bool isNegative() const { return (mantissa[0] & MBF_SIGN_MASK) != 0; }
    
    // Get mantissa without sign bit
    uint32_t getMantissaBits() const {
        return ((mantissa[0] & MBF_MANTISSA_MASK) << 16) |
               (mantissa[1] << 8) |
               mantissa[2];
    }
    
    // Set mantissa bits (without affecting sign)
    void setMantissaBits(uint32_t bits) {
        mantissa[0] = (mantissa[0] & MBF_SIGN_MASK) | ((bits >> 16) & MBF_MANTISSA_MASK);
        mantissa[1] = (bits >> 8) & 0xFF;
        mantissa[2] = bits & 0xFF;
    }
};

/**
 * MBF64 - Microsoft Binary Format Double Precision (8 bytes)
 */
struct MBF64 {
    uint8_t exponent;    // Bias 128, 0 = zero
    uint8_t mantissa[7]; // mantissa[0] has sign bit + 7 mantissa bits
    
    MBF64() : exponent(0), mantissa{0, 0, 0, 0, 0, 0, 0} {}
    MBF64(uint8_t exp, const uint8_t m[7]) : exponent(exp) {
        for (int i = 0; i < 7; i++) mantissa[i] = m[i];
    }
    
    // Construct from byte array
    explicit MBF64(const uint8_t bytes[8]) : exponent(bytes[0]) {
        for (int i = 0; i < 7; i++) mantissa[i] = bytes[i + 1];
    }
    
    // Convert to byte array
    void toBytes(uint8_t bytes[8]) const {
        bytes[0] = exponent;
        for (int i = 0; i < 7; i++) bytes[i + 1] = mantissa[i];
    }
    
    bool isZero() const { return exponent == 0; }
    bool isNegative() const { return (mantissa[0] & MBF_SIGN_MASK) != 0; }
    
    // Get mantissa without sign bit (55 bits stored)
    uint64_t getMantissaBits() const {
        uint64_t bits = mantissa[0] & MBF_MANTISSA_MASK;
        for (int i = 1; i < 7; i++) {
            bits = (bits << 8) | mantissa[i];
        }
        return bits;
    }
    
    // Set mantissa bits (without affecting sign)
    void setMantissaBits(uint64_t bits) {
        mantissa[6] = bits & 0xFF; bits >>= 8;
        mantissa[5] = bits & 0xFF; bits >>= 8;
        mantissa[4] = bits & 0xFF; bits >>= 8;
        mantissa[3] = bits & 0xFF; bits >>= 8;
        mantissa[2] = bits & 0xFF; bits >>= 8;
        mantissa[1] = bits & 0xFF; bits >>= 8;
        mantissa[0] = (mantissa[0] & MBF_SIGN_MASK) | (bits & MBF_MANTISSA_MASK);
    }
};

// Conversion functions
float mbf32ToIEEE(const MBF32& mbf);
double mbf64ToIEEE(const MBF64& mbf);
MBF32 ieeeToMBF32(float ieee);
MBF64 ieeeToMBF64(double ieee);

// Rounding functions (implements GW-BASIC round-to-nearest-even)
MBF32 roundToMBF32(const MBF64& mbf64);
MBF64 normalizeAndRound(uint64_t mantissa, int exponent, bool negative);
MBF32 normalizeAndRound32(uint32_t mantissa, int exponent, bool negative);

// Utility functions
bool isZeroMBF32(const MBF32& mbf);
bool isZeroMBF64(const MBF64& mbf);
int compareMBF32(const MBF32& a, const MBF32& b);
int compareMBF64(const MBF64& a, const MBF64& b);

// String conversion (implements VAL and PRINT semantics)
MBF32 parseNumberToMBF32(const std::string& str);
MBF64 parseNumberToMBF64(const std::string& str);
std::string formatMBF32(const MBF32& mbf);
std::string formatMBF64(const MBF64& mbf);

} // namespace MBF
