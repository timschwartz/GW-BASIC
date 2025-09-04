#pragma once

#include <cstdint>
#include <string>
#include <variant>
#include <optional>

// Microsoft Binary Format (MBF) compatible numeric types
struct Int16 { int16_t v{}; };
struct Single { float v{}; };  // 32-bit MBF single precision
struct Double { double v{}; }; // 64-bit MBF double precision

using NumericValue = std::variant<Int16, Single, Double>;

// Numeric formatting options for PRINT USING and general output
struct FormatOptions {
    bool leadingZeros{false};
    bool trailingZeros{false};
    bool exponentialNotation{false};
    bool thousandsSeparator{false};
    int decimalPlaces{-1}; // -1 means automatic
    int fieldWidth{0};
    char decimalPoint{'.'};
    char thousandsSep{','};
    bool signAlways{false};
    bool signTrailing{false};
};

// Error codes for numeric operations
enum class NumericError {
    None = 0,
    Overflow = 6,      // GW-BASIC error 6
    DivisionByZero = 11, // GW-BASIC error 11
    IllegalFunctionCall = 5, // GW-BASIC error 5
    TypeMismatch = 13  // GW-BASIC error 13
};

// Result type for numeric operations
template<typename T>
struct NumericResult {
    T value{};
    NumericError error{NumericError::None};
    
    explicit operator bool() const { return error == NumericError::None; }
};

class NumericEngine {
public:
    NumericEngine();
    ~NumericEngine() = default;

    // Basic arithmetic operations
    NumericResult<NumericValue> add(const NumericValue& a, const NumericValue& b);
    NumericResult<NumericValue> subtract(const NumericValue& a, const NumericValue& b);
    NumericResult<NumericValue> multiply(const NumericValue& a, const NumericValue& b);
    NumericResult<NumericValue> divide(const NumericValue& a, const NumericValue& b);
    NumericResult<NumericValue> modulo(const NumericValue& a, const NumericValue& b);
    NumericResult<NumericValue> power(const NumericValue& base, const NumericValue& exponent);

    // Comparison operations (returns -1 for true, 0 for false - GW-BASIC style)
    NumericResult<Int16> compare(const NumericValue& a, const NumericValue& b); // <0, 0, >0
    NumericResult<Int16> equals(const NumericValue& a, const NumericValue& b);
    NumericResult<Int16> notEquals(const NumericValue& a, const NumericValue& b);
    NumericResult<Int16> lessThan(const NumericValue& a, const NumericValue& b);
    NumericResult<Int16> lessEqual(const NumericValue& a, const NumericValue& b);
    NumericResult<Int16> greaterThan(const NumericValue& a, const NumericValue& b);
    NumericResult<Int16> greaterEqual(const NumericValue& a, const NumericValue& b);

    // Unary operations
    NumericResult<NumericValue> negate(const NumericValue& a);
    NumericResult<NumericValue> abs(const NumericValue& a);
    NumericResult<Int16> sgn(const NumericValue& a);

    // Math functions
    NumericResult<NumericValue> sqrt(const NumericValue& a);
    NumericResult<NumericValue> log(const NumericValue& a);
    NumericResult<NumericValue> exp(const NumericValue& a);
    NumericResult<NumericValue> sin(const NumericValue& a);
    NumericResult<NumericValue> cos(const NumericValue& a);
    NumericResult<NumericValue> tan(const NumericValue& a);
    NumericResult<NumericValue> atn(const NumericValue& a); // arctan
    
    // Integer operations
    NumericResult<Int16> intFunc(const NumericValue& a); // INT function
    NumericResult<Int16> fix(const NumericValue& a);     // FIX function

    // Random number generation
    NumericResult<Single> rnd(std::optional<NumericValue> seed = std::nullopt);
    void randomize(std::optional<NumericValue> seed = std::nullopt);

    // Type conversion and coercion
    NumericResult<Int16> toInt16(const NumericValue& a);
    NumericResult<Single> toSingle(const NumericValue& a);
    NumericResult<Double> toDouble(const NumericValue& a);
    
    // Promote types for arithmetic (follows GW-BASIC rules)
    NumericValue promoteType(const NumericValue& a, const NumericValue& b);

    // String/numeric conversion (compatible with GW-BASIC VAL/STR$ behavior)
    NumericResult<NumericValue> parseNumber(const std::string& str);
    std::string formatNumber(const NumericValue& value, const FormatOptions& options = {});
    
    // PRINT USING formatting
    std::string printUsing(const std::string& format, const NumericValue& value);

    // Utility functions
    bool isZero(const NumericValue& a);
    bool isNegative(const NumericValue& a);
    bool isInteger(const NumericValue& a);

private:
    uint32_t randomSeed_;
    
    // Internal helper functions
    NumericError checkOverflow(double value, bool isSingle = false);
    NumericValue coerceToCommonType(const NumericValue& a, const NumericValue& b);
    std::string formatFloatGWStyle(double value, const FormatOptions& options);
    std::string formatExponential(double value, const FormatOptions& options);
    
    // Microsoft Binary Format (MBF) compatibility helpers
    float convertToMBFSingle(float ieee754);
    double convertToMBFDouble(double ieee754);
    float convertFromMBFSingle(float mbf);
    double convertFromMBFDouble(double mbf);
};
