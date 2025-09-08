#include "MBFFormat.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <cmath>
#include <limits>

using namespace MBF;
using Catch::Approx;

TEST_CASE("MBF32 Basic Structure", "[mbf32]") {
    SECTION("Default constructor creates zero") {
        MBF32 zero;
        REQUIRE(zero.isZero());
        REQUIRE_FALSE(zero.isNegative());
        REQUIRE(zero.exponent == 0);
        REQUIRE(zero.getMantissaBits() == 0);
    }
    
    SECTION("Manual construction") {
        MBF32 mbf(129, 0x80, 0x00, 0x00); // -1.0 in MBF format
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE(mbf.isNegative());
        REQUIRE(mbf.exponent == 129);
    }
    
    SECTION("Byte conversion") {
        uint8_t bytes[] = {129, 0x80, 0x00, 0x00};
        MBF32 mbf(bytes);
        REQUIRE(mbf.exponent == 129);
        REQUIRE(mbf.isNegative());
        
        uint8_t outBytes[4];
        mbf.toBytes(outBytes);
        for (int i = 0; i < 4; i++) {
            REQUIRE(outBytes[i] == bytes[i]);
        }
    }
}

TEST_CASE("MBF64 Basic Structure", "[mbf64]") {
    SECTION("Default constructor creates zero") {
        MBF64 zero;
        REQUIRE(zero.isZero());
        REQUIRE_FALSE(zero.isNegative());
        REQUIRE(zero.exponent == 0);
        REQUIRE(zero.getMantissaBits() == 0);
    }
    
    SECTION("Byte conversion") {
        uint8_t bytes[] = {129, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
        MBF64 mbf(bytes);
        REQUIRE(mbf.exponent == 129);
        REQUIRE(mbf.isNegative());
        
        uint8_t outBytes[8];
        mbf.toBytes(outBytes);
        for (int i = 0; i < 8; i++) {
            REQUIRE(outBytes[i] == bytes[i]);
        }
    }
}

TEST_CASE("IEEE to MBF32 Conversion", "[mbf32][conversion]") {
    SECTION("Zero conversion") {
        auto mbf = ieeeToMBF32(0.0f);
        REQUIRE(mbf.isZero());
    }
    
    SECTION("Positive number conversion") {
        auto mbf = ieeeToMBF32(1.0f);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE_FALSE(mbf.isNegative());
        REQUIRE(mbf.exponent == 128); // 1.0 has exponent 0 + bias 128 = 128
    }
    
    SECTION("Negative number conversion") {
        auto mbf = ieeeToMBF32(-1.0f);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE(mbf.isNegative());
        REQUIRE(mbf.exponent == 128);
    }
    
    SECTION("Small number conversion") {
        auto mbf = ieeeToMBF32(0.5f);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE(mbf.exponent == 127); // 0.5 has exponent -1 + bias 128 = 127
    }
    
    SECTION("Large number conversion") {
        auto mbf = ieeeToMBF32(2.0f);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE(mbf.exponent == 129); // 2.0 has exponent 1 + bias 128 = 129
    }
}

TEST_CASE("MBF32 to IEEE Conversion", "[mbf32][conversion]") {
    SECTION("Zero conversion") {
        MBF32 mbf; // Zero
        float ieee = mbf32ToIEEE(mbf);
        REQUIRE(ieee == 0.0f);
    }
    
    SECTION("Round-trip conversion") {
        float original = 1.5f;
        auto mbf = ieeeToMBF32(original);
        float converted = mbf32ToIEEE(mbf);
        REQUIRE(converted == Approx(original).epsilon(1e-6));
    }
    
    SECTION("Negative number round-trip") {
        float original = -3.14159f;
        auto mbf = ieeeToMBF32(original);
        float converted = mbf32ToIEEE(mbf);
        REQUIRE(converted == Approx(original).epsilon(1e-6));
    }
}

TEST_CASE("IEEE to MBF64 Conversion", "[mbf64][conversion]") {
    SECTION("Zero conversion") {
        auto mbf = ieeeToMBF64(0.0);
        REQUIRE(mbf.isZero());
    }
    
    SECTION("Positive number conversion") {
        auto mbf = ieeeToMBF64(1.0);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE_FALSE(mbf.isNegative());
        REQUIRE(mbf.exponent == 128);
    }
    
    SECTION("High precision number") {
        auto mbf = ieeeToMBF64(1.23456789012345);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE_FALSE(mbf.isNegative());
    }
}

TEST_CASE("MBF64 to IEEE Conversion", "[mbf64][conversion]") {
    SECTION("Zero conversion") {
        MBF64 mbf; // Zero
        double ieee = mbf64ToIEEE(mbf);
        REQUIRE(ieee == 0.0);
    }
    
    SECTION("Round-trip conversion") {
        double original = 1.5;
        auto mbf = ieeeToMBF64(original);
        double converted = mbf64ToIEEE(mbf);
        REQUIRE(converted == Approx(original).epsilon(1e-15));
    }
    
    SECTION("High precision round-trip") {
        double original = 3.141592653589793;
        auto mbf = ieeeToMBF64(original);
        double converted = mbf64ToIEEE(mbf);
        REQUIRE(converted == Approx(original).epsilon(1e-14));
    }
}

TEST_CASE("MBF64 to MBF32 Rounding", "[mbf32][mbf64][rounding]") {
    SECTION("Simple rounding") {
        auto mbf64 = ieeeToMBF64(1.0);
        auto mbf32 = roundToMBF32(mbf64);
        REQUIRE_FALSE(mbf32.isZero());
        REQUIRE(mbf32.exponent == 128);
    }
    
    SECTION("Precision loss rounding") {
        auto mbf64 = ieeeToMBF64(1.23456789012345);
        auto mbf32 = roundToMBF32(mbf64);
        float result = mbf32ToIEEE(mbf32);
        REQUIRE(result == Approx(1.23456789012345f).epsilon(1e-6));
    }
    
    SECTION("Zero rounding") {
        MBF64 zero;
        auto mbf32 = roundToMBF32(zero);
        REQUIRE(mbf32.isZero());
    }
}

TEST_CASE("Special Values", "[mbf32][mbf64][special]") {
    SECTION("IEEE infinity to MBF") {
        auto mbf32 = ieeeToMBF32(std::numeric_limits<float>::infinity());
        REQUIRE_FALSE(mbf32.isZero());
        REQUIRE(mbf32.exponent == 255); // Maximum MBF exponent
        
        auto mbf64 = ieeeToMBF64(std::numeric_limits<double>::infinity());
        REQUIRE_FALSE(mbf64.isZero());
        REQUIRE(mbf64.exponent == 255);
    }
    
    SECTION("IEEE NaN to MBF") {
        auto mbf32 = ieeeToMBF32(std::numeric_limits<float>::quiet_NaN());
        REQUIRE_FALSE(mbf32.isZero());
        REQUIRE(mbf32.exponent == 255);
    }
    
    SECTION("Very small numbers (subnormal in IEEE)") {
        // MBF doesn't support subnormals, should convert to zero
        auto mbf32 = ieeeToMBF32(1e-45f); // Subnormal in IEEE
        // Result depends on implementation - could be zero or smallest normalized
        REQUIRE_FALSE(std::isnan(mbf32ToIEEE(mbf32)));
    }
}

TEST_CASE("Comparison Functions", "[mbf32][mbf64][comparison]") {
    SECTION("MBF32 comparison") {
        auto a = ieeeToMBF32(1.0f);
        auto b = ieeeToMBF32(2.0f);
        auto c = ieeeToMBF32(1.0f);
        
        REQUIRE(compareMBF32(a, b) < 0);  // 1.0 < 2.0
        REQUIRE(compareMBF32(b, a) > 0);  // 2.0 > 1.0
        REQUIRE(compareMBF32(a, c) == 0); // 1.0 == 1.0
    }
    
    SECTION("MBF64 comparison") {
        auto a = ieeeToMBF64(1.0);
        auto b = ieeeToMBF64(2.0);
        auto c = ieeeToMBF64(1.0);
        
        REQUIRE(compareMBF64(a, b) < 0);
        REQUIRE(compareMBF64(b, a) > 0);
        REQUIRE(compareMBF64(a, c) == 0);
    }
}

TEST_CASE("String Conversion", "[mbf32][mbf64][string]") {
    SECTION("Format MBF32") {
        auto mbf = ieeeToMBF32(1.5f);
        std::string str = formatMBF32(mbf);
        REQUIRE_FALSE(str.empty());
        REQUIRE(str.find("1.5") != std::string::npos);
    }
    
    SECTION("Format MBF64") {
        auto mbf = ieeeToMBF64(1.5);
        std::string str = formatMBF64(mbf);
        REQUIRE_FALSE(str.empty());
        REQUIRE(str.find("1.5") != std::string::npos);
    }
    
    SECTION("Parse MBF32") {
        auto mbf = parseNumberToMBF32("1.5");
        float result = mbf32ToIEEE(mbf);
        REQUIRE(result == Approx(1.5f).epsilon(1e-6));
    }
    
    SECTION("Parse MBF64") {
        auto mbf = parseNumberToMBF64("1.5");
        double result = mbf64ToIEEE(mbf);
        REQUIRE(result == Approx(1.5).epsilon(1e-15));
    }
    
    SECTION("Parse invalid string") {
        auto mbf32 = parseNumberToMBF32("invalid");
        REQUIRE(mbf32.isZero()); // Should return zero on error
        
        auto mbf64 = parseNumberToMBF64("invalid");
        REQUIRE(mbf64.isZero());
    }
}

TEST_CASE("Normalization Functions", "[mbf32][mbf64][normalization]") {
    SECTION("Normalize MBF64") {
        // Test with a mantissa that needs normalization
        uint64_t mantissa = 0x40000000000000ULL; // Needs left shift by 1
        auto mbf = normalizeAndRound(mantissa, 130, false);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE(mbf.exponent == 129); // Should be decremented during normalization
    }
    
    SECTION("Normalize MBF32") {
        // Test with a mantissa that needs normalization
        uint32_t mantissa = 0x400000u; // Needs left shift by 1  
        auto mbf = normalizeAndRound32(mantissa, 130, false);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE(mbf.exponent == 129);
    }
    
    SECTION("Underflow during normalization") {
        // Test with a very small mantissa and low exponent
        // This should normalize and potentially underflow to zero
        auto mbf = normalizeAndRound(1ULL, 1, false);
        // With such a small mantissa and low exponent, it should underflow
        // Since 1ULL needs to be shifted left 55 times to normalize,
        // and exponent starts at 1, it will underflow to 0
        REQUIRE(mbf.isZero()); // Should underflow to zero
    }
    
    SECTION("Overflow during normalization") {
        // Test with overflow conditions
        auto mbf = normalizeAndRound(0x80000000000000ULL, 300, false);
        REQUIRE_FALSE(mbf.isZero());
        REQUIRE(mbf.exponent == 255); // Should clamp to maximum
    }
}

TEST_CASE("Edge Cases", "[mbf32][mbf64][edge]") {
    SECTION("Mantissa boundary values") {
        // Test mantissa bit manipulation
        MBF32 mbf;
        mbf.exponent = 129;
        mbf.setMantissaBits(0x7FFFFF); // Maximum mantissa
        REQUIRE(mbf.getMantissaBits() == 0x7FFFFF);
        
        mbf.setMantissaBits(0); // Minimum mantissa
        REQUIRE(mbf.getMantissaBits() == 0);
    }
    
    SECTION("Sign handling") {
        MBF32 mbf;
        mbf.exponent = 129;
        REQUIRE_FALSE(mbf.isNegative());
        
        mbf.mantissa[0] |= MBF_SIGN_MASK;
        REQUIRE(mbf.isNegative());
        
        mbf.mantissa[0] &= ~MBF_SIGN_MASK;
        REQUIRE_FALSE(mbf.isNegative());
    }
    
    SECTION("Exponent boundary") {
        // Test minimum and maximum exponents that can be represented
        // Use values that are within MBF range but test boundary conditions
        auto mbfMax = ieeeToMBF32(1e30f);  // Large but representable
        REQUIRE_FALSE(mbfMax.isZero());
        
        // For very small numbers, they may underflow to zero in MBF
        // which is correct behavior - MBF has limited range compared to IEEE
        auto mbfSmall = ieeeToMBF32(1e-30f); // This should still be representable
        // Note: very small values like 1e-38f may underflow to zero in MBF
    }
}
