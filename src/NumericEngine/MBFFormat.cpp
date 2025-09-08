#include "MBFFormat.hpp"
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <limits>
#include <cstring>

namespace MBF {

/**
 * Convert MBF32 to IEEE 754 single precision
 */
float mbf32ToIEEE(const MBF32& mbf) {
    // Handle zero
    if (mbf.isZero()) {
        return 0.0f;
    }
    
    // Extract components
    bool negative = mbf.isNegative();
    int mbfExponent = mbf.exponent;
    uint32_t mbfMantissa = mbf.getMantissaBits();
    
    // Convert exponent from MBF bias (128) to IEEE bias (127)
    int ieeeExponent = mbfExponent - MBF_EXPONENT_BIAS + IEEE_SINGLE_EXPONENT_BIAS;
    
    // Check for IEEE overflow/underflow
    if (ieeeExponent >= 255) {
        // Overflow to infinity
        return negative ? -std::numeric_limits<float>::infinity() 
                        : std::numeric_limits<float>::infinity();
    }
    if (ieeeExponent <= 0) {
        // Underflow to zero (MBF doesn't support subnormals)
        return negative ? -0.0f : 0.0f;
    }
    
    // Convert mantissa from MBF format to IEEE format
    // Both formats store the same 23 mantissa bits (both have implicit leading 1)
    uint32_t ieeeMantissa = mbfMantissa;
    
    // Construct IEEE 754 representation
    uint32_t ieeeWord = 0;
    if (negative) ieeeWord |= 0x80000000u;
    ieeeWord |= (static_cast<uint32_t>(ieeeExponent) << 23);
    ieeeWord |= (ieeeMantissa & 0x7FFFFFu);
    
    // Reinterpret as float
    float result;
    std::memcpy(&result, &ieeeWord, sizeof(float));
    return result;
}

/**
 * Convert MBF64 to IEEE 754 double precision
 */
double mbf64ToIEEE(const MBF64& mbf) {
    // Handle zero
    if (mbf.isZero()) {
        return 0.0;
    }
    
    // Extract components
    bool negative = mbf.isNegative();
    int mbfExponent = mbf.exponent;
    uint64_t mbfMantissa = mbf.getMantissaBits();
    
    // Convert exponent from MBF bias (128) to IEEE bias (1023)
    int ieeeExponent = mbfExponent - MBF_EXPONENT_BIAS + IEEE_DOUBLE_EXPONENT_BIAS;
    
    // Check for IEEE overflow/underflow
    if (ieeeExponent >= 2047) {
        // Overflow to infinity
        return negative ? -std::numeric_limits<double>::infinity() 
                        : std::numeric_limits<double>::infinity();
    }
    if (ieeeExponent <= 0) {
        // Underflow to zero (MBF doesn't support subnormals)
        return negative ? -0.0 : 0.0;
    }
    
    // Convert mantissa from MBF format (55 stored bits) to IEEE format (52 stored bits)
    // Need to round down from 55 to 52 bits
    // Round to nearest, ties to even
    uint64_t ieeeMantissa;
    if (mbfMantissa >= (1ULL << 55)) {
        // Mantissa overflow, should not happen with normalized input
        ieeeMantissa = (1ULL << 52) - 1;
    } else {
        // Round 55-bit mantissa to 52 bits
        uint64_t roundBit = (mbfMantissa >> 2) & 1;
        uint64_t stickyBits = mbfMantissa & 0x3;
        ieeeMantissa = mbfMantissa >> 3;
        
        // Round to nearest, ties to even
        if (roundBit && (stickyBits || (ieeeMantissa & 1))) {
            ieeeMantissa++;
            if (ieeeMantissa >= (1ULL << 52)) {
                // Mantissa overflow - increment exponent
                ieeeMantissa = 0;
                ieeeExponent++;
                if (ieeeExponent >= 2047) {
                    return negative ? -std::numeric_limits<double>::infinity() 
                                    : std::numeric_limits<double>::infinity();
                }
            }
        }
    }
    
    // Construct IEEE 754 representation
    uint64_t ieeeWord = 0;
    if (negative) ieeeWord |= 0x8000000000000000ULL;
    ieeeWord |= (static_cast<uint64_t>(ieeeExponent) << 52);
    ieeeWord |= (ieeeMantissa & 0xFFFFFFFFFFFFFULL);
    
    // Reinterpret as double
    double result;
    std::memcpy(&result, &ieeeWord, sizeof(double));
    return result;
}

/**
 * Convert IEEE 754 single precision to MBF32
 */
MBF32 ieeeToMBF32(float ieee) {
    // Handle zero and special cases
    if (ieee == 0.0f) {
        return MBF32();
    }
    
    // Extract IEEE components
    uint32_t ieeeWord;
    std::memcpy(&ieeeWord, &ieee, sizeof(float));
    
    bool negative = (ieeeWord & 0x80000000u) != 0;
    int ieeeExponent = (ieeeWord >> 23) & 0xFF;
    uint32_t ieeeMantissa = ieeeWord & 0x7FFFFFu;
    
    // Handle IEEE special cases
    if (ieeeExponent == 255) {
        // IEEE infinity or NaN - convert to largest MBF value
        MBF32 result;
        result.exponent = 255; // Largest possible MBF exponent
        result.setMantissaBits(0x7FFFFF); // Largest mantissa
        if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
        return result;
    }
    
    if (ieeeExponent == 0) {
        // IEEE zero or subnormal - convert to zero (MBF doesn't support subnormals)
        return MBF32();
    }
    
    // Convert exponent from IEEE bias (127) to MBF bias (128)
    int mbfExponent = ieeeExponent - IEEE_SINGLE_EXPONENT_BIAS + MBF_EXPONENT_BIAS;
    
    // Check for MBF overflow/underflow
    if (mbfExponent >= 256) {
        // Overflow - return largest MBF value
        MBF32 result;
        result.exponent = 255;
        result.setMantissaBits(0x7FFFFF);
        if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
        return result;
    }
    if (mbfExponent <= 0) {
        // Underflow to zero
        return MBF32();
    }
    
    // For normalized IEEE numbers, the mantissa already excludes the implicit leading 1
    // Both IEEE and MBF store the same 23 mantissa bits (implicit leading 1)
    uint32_t mbfMantissa = ieeeMantissa;
    
    // Create result
    MBF32 result;
    result.exponent = static_cast<uint8_t>(mbfExponent);
    result.setMantissaBits(mbfMantissa);
    if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
    
    return result;
}

/**
 * Convert IEEE 754 double precision to MBF64
 */
MBF64 ieeeToMBF64(double ieee) {
    // Handle zero and special cases
    if (ieee == 0.0) {
        return MBF64();
    }
    
    // Extract IEEE components
    uint64_t ieeeWord;
    std::memcpy(&ieeeWord, &ieee, sizeof(double));
    
    bool negative = (ieeeWord & 0x8000000000000000ULL) != 0;
    int ieeeExponent = (ieeeWord >> 52) & 0x7FF;
    uint64_t ieeeMantissa = ieeeWord & 0xFFFFFFFFFFFFFULL;
    
    // Handle IEEE special cases
    if (ieeeExponent == 2047) {
        // IEEE infinity or NaN - convert to largest MBF value
        MBF64 result;
        result.exponent = 255; // Largest possible MBF exponent
        result.setMantissaBits(0x7FFFFFFFFFFFFFULL); // Largest mantissa (55 bits)
        if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
        return result;
    }
    
    if (ieeeExponent == 0) {
        // IEEE zero or subnormal - convert to zero (MBF doesn't support subnormals)
        return MBF64();
    }
    
    // Convert exponent from IEEE bias (1023) to MBF bias (128)
    int mbfExponent = ieeeExponent - IEEE_DOUBLE_EXPONENT_BIAS + MBF_EXPONENT_BIAS;
    
    // Check for MBF overflow/underflow
    if (mbfExponent >= 256) {
        // Overflow - return largest MBF value
        MBF64 result;
        result.exponent = 255;
        result.setMantissaBits(0x7FFFFFFFFFFFFFULL);
        if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
        return result;
    }
    if (mbfExponent <= 0) {
        // Underflow to zero
        return MBF64();
    }
    
    // Convert mantissa from IEEE format (52 stored bits) to MBF format (55 stored bits)
    // Shift left by 3 bits to go from 52 to 55 bits
    uint64_t mbfMantissa = ieeeMantissa << 3;
    
    // Create result
    MBF64 result;
    result.exponent = static_cast<uint8_t>(mbfExponent);
    result.setMantissaBits(mbfMantissa);
    if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
    
    return result;
}

/**
 * Round MBF64 to MBF32 precision (implements round-to-nearest-even)
 */
MBF32 roundToMBF32(const MBF64& mbf64) {
    if (mbf64.isZero()) {
        return MBF32();
    }
    
    bool negative = mbf64.isNegative();
    int exponent = mbf64.exponent;
    uint64_t mantissa64 = mbf64.getMantissaBits();
    
    // Convert from 55-bit to 23-bit mantissa
    // Need to round down from 55 to 23 bits (32 bits to remove)
    uint32_t roundingBits = mantissa64 & 0xFFFFFFFFu; // Lower 32 bits
    uint32_t mantissa32 = static_cast<uint32_t>(mantissa64 >> 32) & 0x7FFFFF; // Upper 23 bits
    
    // Round to nearest, ties to even
    bool roundUp = false;
    if (roundingBits > 0x80000000u) {
        // More than half - round up
        roundUp = true;
    } else if (roundingBits == 0x80000000u) {
        // Exactly half - round to even
        roundUp = (mantissa32 & 1) != 0;
    }
    
    if (roundUp) {
        mantissa32++;
        if (mantissa32 >= 0x800000u) {
            // Mantissa overflow - increment exponent
            mantissa32 = 0;
            exponent++;
            if (exponent >= 256) {
                // Exponent overflow - return largest MBF32
                MBF32 result;
                result.exponent = 255;
                result.setMantissaBits(0x7FFFFF);
                if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
                return result;
            }
        }
    }
    
    // Create result
    MBF32 result;
    result.exponent = static_cast<uint8_t>(exponent);
    result.setMantissaBits(mantissa32);
    if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
    
    return result;
}

/**
 * Normalize and round a raw mantissa/exponent to MBF64
 */
MBF64 normalizeAndRound(uint64_t mantissa, int exponent, bool negative) {
    if (mantissa == 0) {
        return MBF64();
    }
    
    // Normalize mantissa to have leading 1 in bit 55 (0-indexed from right)
    // This means the mantissa should be >= 2^55
    while (mantissa < (1ULL << 55) && exponent > 1) {
        mantissa <<= 1;
        exponent--;
    }
    
    // Check for underflow
    if (exponent <= 0 || mantissa < (1ULL << 55)) {
        return MBF64();
    }
    
    // Check for overflow
    if (exponent >= 256) {
        MBF64 result;
        result.exponent = 255;
        result.setMantissaBits(0x7FFFFFFFFFFFFFULL);
        if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
        return result;
    }
    
    // Remove the implicit leading 1 bit (bit 55)
    mantissa &= 0x7FFFFFFFFFFFFFULL; // Keep only the 55 stored bits
    
    // Create result
    MBF64 result;
    result.exponent = static_cast<uint8_t>(exponent);
    result.setMantissaBits(mantissa);
    if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
    
    return result;
}

/**
 * Normalize and round a raw mantissa/exponent to MBF32
 */
MBF32 normalizeAndRound32(uint32_t mantissa, int exponent, bool negative) {
    if (mantissa == 0) {
        return MBF32();
    }
    
    // Normalize mantissa to have leading 1 in bit 23 (0-indexed from right)
    // This means the mantissa should be >= 2^23
    while (mantissa < (1u << 23) && exponent > 1) {
        mantissa <<= 1;
        exponent--;
    }
    
    // Check for underflow
    if (exponent <= 0 || mantissa < (1u << 23)) {
        return MBF32();
    }
    
    // Check for overflow
    if (exponent >= 256) {
        MBF32 result;
        result.exponent = 255;
        result.setMantissaBits(0x7FFFFF);
        if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
        return result;
    }
    
    // Remove the implicit leading 1 bit (bit 23)
    mantissa &= 0x7FFFFFu; // Keep only the 23 stored bits
    
    // Create result
    MBF32 result;
    result.exponent = static_cast<uint8_t>(exponent);
    result.setMantissaBits(mantissa);
    if (negative) result.mantissa[0] |= MBF_SIGN_MASK;
    
    return result;
}

/**
 * Utility functions
 */
bool isZeroMBF32(const MBF32& mbf) {
    return mbf.isZero();
}

bool isZeroMBF64(const MBF64& mbf) {
    return mbf.isZero();
}

int compareMBF32(const MBF32& a, const MBF32& b) {
    // Convert to IEEE and compare (simple but correct)
    float fa = mbf32ToIEEE(a);
    float fb = mbf32ToIEEE(b);
    
    if (fa < fb) return -1;
    if (fa > fb) return 1;
    return 0;
}

int compareMBF64(const MBF64& a, const MBF64& b) {
    // Convert to IEEE and compare (simple but correct)
    double da = mbf64ToIEEE(a);
    double db = mbf64ToIEEE(b);
    
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/**
 * Parse string to MBF32 (implements VAL semantics)
 */
MBF32 parseNumberToMBF32(const std::string& str) {
    // Simple implementation - convert via IEEE float
    // TODO: Implement exact GW-BASIC VAL semantics from documentation
    try {
        float value = std::stof(str);
        return ieeeToMBF32(value);
    } catch (...) {
        return MBF32(); // Return zero on error
    }
}

/**
 * Parse string to MBF64 (implements VAL semantics)
 */
MBF64 parseNumberToMBF64(const std::string& str) {
    // Simple implementation - convert via IEEE double
    // TODO: Implement exact GW-BASIC VAL semantics from documentation
    try {
        double value = std::stod(str);
        return ieeeToMBF64(value);
    } catch (...) {
        return MBF64(); // Return zero on error
    }
}

/**
 * Format MBF32 to string (implements PRINT semantics)
 */
std::string formatMBF32(const MBF32& mbf) {
    // Simple implementation - convert via IEEE float
    // TODO: Implement exact GW-BASIC PRINT semantics from documentation
    float value = mbf32ToIEEE(mbf);
    
    if (value == 0.0f) {
        return " 0";
    }
    
    std::ostringstream oss;
    
    // Add leading space for positive numbers (GW-BASIC style)
    if (value >= 0.0f) {
        oss << " ";
    }
    
    // Choose between fixed and exponential notation
    if (std::abs(value) >= 1e7f || (std::abs(value) < 1e-6f && value != 0.0f)) {
        oss << std::scientific << std::setprecision(6) << value;
        
        // Convert 'e' to 'E' and ensure proper format
        std::string result = oss.str();
        size_t ePos = result.find('e');
        if (ePos != std::string::npos) {
            result[ePos] = 'E';
        }
        return result;
    } else {
        oss << value;
        return oss.str();
    }
}

/**
 * Format MBF64 to string (implements PRINT semantics)
 */
std::string formatMBF64(const MBF64& mbf) {
    // Simple implementation - convert via IEEE double
    // TODO: Implement exact GW-BASIC PRINT semantics from documentation
    double value = mbf64ToIEEE(mbf);
    
    if (value == 0.0) {
        return " 0";
    }
    
    std::ostringstream oss;
    
    // Add leading space for positive numbers (GW-BASIC style)
    if (value >= 0.0) {
        oss << " ";
    }
    
    // Choose between fixed and exponential notation
    if (std::abs(value) >= 1e15 || (std::abs(value) < 1e-15 && value != 0.0)) {
        oss << std::scientific << std::setprecision(15) << value;
        
        // Convert 'e' to 'E' and ensure proper format
        std::string result = oss.str();
        size_t ePos = result.find('e');
        if (ePos != std::string::npos) {
            result[ePos] = 'E';
        }
        return result;
    } else {
        oss << std::setprecision(15) << value;
        return oss.str();
    }
}

} // namespace MBF
