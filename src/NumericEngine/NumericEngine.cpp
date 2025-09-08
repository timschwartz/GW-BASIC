#include "NumericEngine.hpp"

#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>
#include <optional>

NumericEngine::NumericEngine() {
    // Initialize random seed with current time
    auto now = std::chrono::high_resolution_clock::now();
    auto duration = now.time_since_epoch();
    randomSeed_ = static_cast<uint32_t>(duration.count() & 0xFFFFFFFF);
}

// Basic arithmetic operations
NumericResult<NumericValue> NumericEngine::add(const NumericValue& a, const NumericValue& b) {
    return std::visit([this, &a, &b](auto&& x, auto&& y) -> NumericResult<NumericValue> {
        using T1 = std::decay_t<decltype(x)>;
        using T2 = std::decay_t<decltype(y)>;
        
        if constexpr (std::is_same_v<T1, Int16> && std::is_same_v<T2, Int16>) {
            long result = static_cast<long>(x.v) + static_cast<long>(y.v);
            if (result < -32768 || result > 32767) {
                return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
            }
            return {NumericValue{Int16{static_cast<int16_t>(result)}}, NumericError::None};
        } else {
            // Promote to higher precision type
            double dx = std::visit([](auto&& val) -> double {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, Int16>) return val.v;
                if constexpr (std::is_same_v<T, Single>) return val.v;
                if constexpr (std::is_same_v<T, Double>) return val.v;
                return 0.0;
            }, a);
            
            double dy = std::visit([](auto&& val) -> double {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, Int16>) return val.v;
                if constexpr (std::is_same_v<T, Single>) return val.v;
                if constexpr (std::is_same_v<T, Double>) return val.v;
                return 0.0;
            }, b);
            
            double result = dx + dy;
            auto error = checkOverflow(result, 
                std::holds_alternative<Single>(a) || std::holds_alternative<Single>(b));
            
            if (error != NumericError::None) {
                return {NumericValue{Double{0.0}}, error};
            }
            
            // Return in highest precision type of the operands
            if (std::holds_alternative<Double>(a) || std::holds_alternative<Double>(b)) {
                return {NumericValue{Double{result}}, NumericError::None};
            } else {
                return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
            }
        }
    }, a, b);
}

NumericResult<NumericValue> NumericEngine::subtract(const NumericValue& a, const NumericValue& b) {
    return std::visit([this, &a, &b](auto&& x, auto&& y) -> NumericResult<NumericValue> {
        using T1 = std::decay_t<decltype(x)>;
        using T2 = std::decay_t<decltype(y)>;
        
        if constexpr (std::is_same_v<T1, Int16> && std::is_same_v<T2, Int16>) {
            long result = static_cast<long>(x.v) - static_cast<long>(y.v);
            if (result < -32768 || result > 32767) {
                return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
            }
            return {NumericValue{Int16{static_cast<int16_t>(result)}}, NumericError::None};
        } else {
            double dx = std::visit([](auto&& val) -> double {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, Int16>) return val.v;
                if constexpr (std::is_same_v<T, Single>) return val.v;
                if constexpr (std::is_same_v<T, Double>) return val.v;
                return 0.0;
            }, a);
            
            double dy = std::visit([](auto&& val) -> double {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, Int16>) return val.v;
                if constexpr (std::is_same_v<T, Single>) return val.v;
                if constexpr (std::is_same_v<T, Double>) return val.v;
                return 0.0;
            }, b);
            
            double result = dx - dy;
            auto error = checkOverflow(result, 
                std::holds_alternative<Single>(a) || std::holds_alternative<Single>(b));
            
            if (error != NumericError::None) {
                return {NumericValue{Double{0.0}}, error};
            }
            
            if (std::holds_alternative<Double>(a) || std::holds_alternative<Double>(b)) {
                return {NumericValue{Double{result}}, NumericError::None};
            } else {
                return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
            }
        }
    }, a, b);
}

NumericResult<NumericValue> NumericEngine::multiply(const NumericValue& a, const NumericValue& b) {
    return std::visit([this, &a, &b](auto&& x, auto&& y) -> NumericResult<NumericValue> {
        using T1 = std::decay_t<decltype(x)>;
        using T2 = std::decay_t<decltype(y)>;
        
        if constexpr (std::is_same_v<T1, Int16> && std::is_same_v<T2, Int16>) {
            long result = static_cast<long>(x.v) * static_cast<long>(y.v);
            if (result < -32768 || result > 32767) {
                return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
            }
            return {NumericValue{Int16{static_cast<int16_t>(result)}}, NumericError::None};
        } else {
            double dx = std::visit([](auto&& val) -> double {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, Int16>) return val.v;
                if constexpr (std::is_same_v<T, Single>) return val.v;
                if constexpr (std::is_same_v<T, Double>) return val.v;
                return 0.0;
            }, a);
            
            double dy = std::visit([](auto&& val) -> double {
                using T = std::decay_t<decltype(val)>;
                if constexpr (std::is_same_v<T, Int16>) return val.v;
                if constexpr (std::is_same_v<T, Single>) return val.v;
                if constexpr (std::is_same_v<T, Double>) return val.v;
                return 0.0;
            }, b);
            
            double result = dx * dy;
            auto error = checkOverflow(result, 
                std::holds_alternative<Single>(a) || std::holds_alternative<Single>(b));
            
            if (error != NumericError::None) {
                return {NumericValue{Double{0.0}}, error};
            }
            
            if (std::holds_alternative<Double>(a) || std::holds_alternative<Double>(b)) {
                return {NumericValue{Double{result}}, NumericError::None};
            } else {
                return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
            }
        }
    }, a, b);
}

NumericResult<NumericValue> NumericEngine::divide(const NumericValue& a, const NumericValue& b) {
    // Check for division by zero first
    if (isZero(b)) {
        return {NumericValue{Double{0.0}}, NumericError::DivisionByZero};
    }
    
    return std::visit([this, &a, &b]([[maybe_unused]] auto&& x, [[maybe_unused]] auto&& y) -> NumericResult<NumericValue> {
        double dx = std::visit([](auto&& val) -> double {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, Int16>) return val.v;
            if constexpr (std::is_same_v<T, Single>) return val.v;
            if constexpr (std::is_same_v<T, Double>) return val.v;
            return 0.0;
        }, a);
        
        double dy = std::visit([](auto&& val) -> double {
            using T = std::decay_t<decltype(val)>;
            if constexpr (std::is_same_v<T, Int16>) return val.v;
            if constexpr (std::is_same_v<T, Single>) return val.v;
            if constexpr (std::is_same_v<T, Double>) return val.v;
            return 0.0;
        }, b);
        
        double result = dx / dy;
        auto error = checkOverflow(result, 
            std::holds_alternative<Single>(a) || std::holds_alternative<Single>(b));
        
        if (error != NumericError::None) {
            return {NumericValue{Double{0.0}}, error};
        }
        
        // Division always promotes to floating point
        if (std::holds_alternative<Double>(a) || std::holds_alternative<Double>(b)) {
            return {NumericValue{Double{result}}, NumericError::None};
        } else {
            return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
        }
    }, a, b);
}

NumericResult<NumericValue> NumericEngine::power(const NumericValue& base, const NumericValue& exponent) {
    double baseVal = std::visit([](auto&& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, Int16>) return val.v;
        if constexpr (std::is_same_v<T, Single>) return val.v;
        if constexpr (std::is_same_v<T, Double>) return val.v;
        return 0.0;
    }, base);
    
    double expVal = std::visit([](auto&& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, Int16>) return val.v;
        if constexpr (std::is_same_v<T, Single>) return val.v;
        if constexpr (std::is_same_v<T, Double>) return val.v;
        return 0.0;
    }, exponent);
    
    // Handle special cases
    if (baseVal == 0.0 && expVal < 0.0) {
        return {NumericValue{Double{0.0}}, NumericError::DivisionByZero};
    }
    
    if (baseVal < 0.0 && std::floor(expVal) != expVal) {
        return {NumericValue{Double{0.0}}, NumericError::IllegalFunctionCall};
    }
    
    double result = std::pow(baseVal, expVal);
    
    if (!std::isfinite(result)) {
        return {NumericValue{Double{0.0}}, NumericError::Overflow};
    }
    
    auto error = checkOverflow(result, 
        std::holds_alternative<Single>(base) || std::holds_alternative<Single>(exponent));
    
    if (error != NumericError::None) {
        return {NumericValue{Double{0.0}}, error};
    }
    
    // Return in highest precision type
    if (std::holds_alternative<Double>(base) || std::holds_alternative<Double>(exponent)) {
        return {NumericValue{Double{result}}, NumericError::None};
    } else {
        return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
    }
}

// Comparison operations
NumericResult<Int16> NumericEngine::compare(const NumericValue& a, const NumericValue& b) {
    double da = std::visit([](auto&& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, Int16>) return val.v;
        if constexpr (std::is_same_v<T, Single>) return val.v;
        if constexpr (std::is_same_v<T, Double>) return val.v;
        return 0.0;
    }, a);
    
    double db = std::visit([](auto&& val) -> double {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, Int16>) return val.v;
        if constexpr (std::is_same_v<T, Single>) return val.v;
        if constexpr (std::is_same_v<T, Double>) return val.v;
        return 0.0;
    }, b);
    
    if (da < db) return {Int16{-1}, NumericError::None};
    if (da > db) return {Int16{1}, NumericError::None};
    return {Int16{0}, NumericError::None};
}

NumericResult<Int16> NumericEngine::equals(const NumericValue& a, const NumericValue& b) {
    auto cmp = compare(a, b);
    if (!cmp) return cmp;
    int16_t result = (cmp.value.v == 0) ? static_cast<int16_t>(-1) : static_cast<int16_t>(0);
    return {Int16{result}, NumericError::None};  // GW-BASIC true = -1
}

// Math functions
NumericResult<NumericValue> NumericEngine::sqrt(const NumericValue& a) {
    double val = std::visit([](auto&& x) -> double {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Int16>) return x.v;
        if constexpr (std::is_same_v<T, Single>) return x.v;
        if constexpr (std::is_same_v<T, Double>) return x.v;
        return 0.0;
    }, a);
    
    if (val < 0.0) {
        return {NumericValue{Double{0.0}}, NumericError::IllegalFunctionCall};
    }
    
    double result = std::sqrt(val);
    
    if (std::holds_alternative<Double>(a)) {
        return {NumericValue{Double{result}}, NumericError::None};
    } else {
        return {NumericValue{Single{static_cast<float>(result)}}, NumericError::None};
    }
}

NumericResult<Single> NumericEngine::rnd(std::optional<NumericValue> seed) {
    static std::mt19937 generator(randomSeed_);
    static std::uniform_real_distribution<float> distribution(0.0f, 1.0f);
    
    if (seed.has_value()) {
        double seedVal = std::visit([](auto&& x) -> double {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, Int16>) return x.v;
            if constexpr (std::is_same_v<T, Single>) return x.v;
            if constexpr (std::is_same_v<T, Double>) return x.v;
            return 0.0;
        }, seed.value());
        
        if (seedVal < 0) {
            // Negative seed: reseed with system time
            auto now = std::chrono::high_resolution_clock::now();
            auto duration = now.time_since_epoch();
            generator.seed(static_cast<uint32_t>(duration.count() & 0xFFFFFFFF));
        } else if (seedVal > 0) {
            // Positive seed: reseed with provided value
            generator.seed(static_cast<uint32_t>(seedVal));
        }
        // Zero seed: use current sequence
    }
    
    return {Single{distribution(generator)}, NumericError::None};
}

// String/numeric conversion
std::string NumericEngine::formatNumber(const NumericValue& value, const FormatOptions& options) {
    return std::visit([&](auto&& x) -> std::string {
        using T = std::decay_t<decltype(x)>;
        
        if constexpr (std::is_same_v<T, Int16>) {
            std::ostringstream oss;
            if (options.signAlways && x.v >= 0) oss << "+";
            oss << x.v;
            return oss.str();
        } else {
            double val = (std::is_same_v<T, Single>) ? x.v : x.v;
            return formatFloatGWStyle(val, options);
        }
    }, value);
}

// Utility functions
bool NumericEngine::isZero(const NumericValue& a) {
    return std::visit([](auto&& x) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Int16>) return x.v == 0;
        if constexpr (std::is_same_v<T, Single>) return x.v == 0.0f;
        if constexpr (std::is_same_v<T, Double>) return x.v == 0.0;
        return false;
    }, a);
}

bool NumericEngine::isNegative(const NumericValue& a) {
    return std::visit([](auto&& x) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Int16>) return x.v < 0;
        if constexpr (std::is_same_v<T, Single>) return x.v < 0.0f;
        if constexpr (std::is_same_v<T, Double>) return x.v < 0.0;
        return false;
    }, a);
}

// Private helper functions
NumericError NumericEngine::checkOverflow(double value, bool isSingle) {
    if (!std::isfinite(value)) {
        return NumericError::Overflow;
    }
    
    if (isSingle) {
        // Single precision limits (approximate MBF limits)
        if (std::abs(value) > 3.4e38) {
            return NumericError::Overflow;
        }
    } else {
        // Double precision limits (approximate MBF limits)  
        if (std::abs(value) > 1.7e308) {
            return NumericError::Overflow;
        }
    }
    
    return NumericError::None;
}

std::string NumericEngine::printUsing(const std::string& format, const NumericValue& value) {
    // Parse the format string and format the value according to GW-BASIC PRINT USING rules
    if (format.empty()) {
        return formatNumber(value);
    }
    
    size_t pos = 0;
    std::string result;
    
    // Look for numeric format patterns
    if (auto numFormat = parseNumericFormat(format, pos)) {
        return formatWithNumericPattern(*numFormat, value);
    }
    
    // Look for string format patterns  
    if (auto strFormat = parseStringFormat(format, pos)) {
        // Convert numeric value to string for string formatting
        std::string strValue = formatNumber(value);
        return formatWithStringPattern(*strFormat, strValue);
    }
    
    // If no format found, return the value formatted normally
    return formatNumber(value);
}

std::string NumericEngine::formatFloatGWStyle(double value, const FormatOptions& options) {
    std::ostringstream oss;
    
    if (options.exponentialNotation || std::abs(value) >= 1e7 || (std::abs(value) < 1e-6 && value != 0.0)) {
        return formatExponential(value, options);
    }
    
    // Regular decimal notation
    if (options.decimalPlaces >= 0) {
        oss << std::fixed << std::setprecision(options.decimalPlaces);
    }
    
    if (options.signAlways && value >= 0) {
        oss << "+";
    }
    
    oss << value;
    
    std::string result = oss.str();
    
    // Add leading space for positive numbers (GW-BASIC style)
    if (value >= 0 && !options.signAlways && result[0] != ' ') {
        result = " " + result;
    }
    
    return result;
}

std::string NumericEngine::formatExponential(double value, const FormatOptions& options) {
    std::ostringstream oss;
    
    if (options.signAlways && value >= 0) {
        oss << "+";
    }
    
    // GW-BASIC style exponential notation
    oss << std::scientific << std::setprecision(6) << value;
    
    std::string result = oss.str();
    
    // Convert 'e' to 'E' and adjust format to match GW-BASIC
    size_t ePos = result.find('e');
    if (ePos != std::string::npos) {
        result[ePos] = 'E';
        
        // Ensure two-digit exponent
        if (ePos + 2 < result.length() && result[ePos + 2] != '+' && result[ePos + 2] != '-') {
            result.insert(ePos + 2, "+0");
        }
    }
    
    return result;
}

// Placeholder implementations for MBF conversion (would need actual MBF format implementation)
float NumericEngine::convertToMBFSingle(float ieee754) {
    // TODO: Implement actual MBF conversion
    return ieee754;
}

double NumericEngine::convertToMBFDouble(double ieee754) {
    // TODO: Implement actual MBF conversion
    return ieee754;
}

float NumericEngine::convertFromMBFSingle(float mbf) {
    // TODO: Implement actual MBF conversion
    return mbf;
}

double NumericEngine::convertFromMBFDouble(double mbf) {
    // TODO: Implement actual MBF conversion
    return mbf;
}

// PRINT USING helper functions
std::optional<NumericPattern> NumericEngine::parseNumericFormat(const std::string& format, size_t& pos) {
    NumericPattern pattern;
    size_t start = pos;
    bool foundDigits = false;
    
    // Look for leading sign
    if (pos < format.length() && format[pos] == '+') {
        pattern.leadingSign = true;
        ++pos;
    }
    
    // Look for floating dollar signs $$
    if (pos + 1 < format.length() && format[pos] == '$' && format[pos + 1] == '$') {
        pattern.floatDollar = true;
        pos += 2;
    } else if (pos < format.length() && format[pos] == '$') {
        pattern.dollarSign = true;
        ++pos;
    }
    
    // Look for asterisk fill
    if (pos < format.length() && format[pos] == '*') {
        pattern.asteriskFill = true;
        ++pos;
        // Skip additional asterisks
        while (pos < format.length() && format[pos] == '*') ++pos;
    }
    
    // Count digits before decimal point
    while (pos < format.length() && format[pos] == '#') {
        pattern.digitsBefore++;
        foundDigits = true;
        ++pos;
    }
    
    // Check for comma (thousands separator)
    if (pos < format.length() && format[pos] == ',') {
        pattern.hasCommas = true;
        ++pos;
        // Continue counting digits with commas
        while (pos < format.length()) {
            if (format[pos] == '#') {
                pattern.digitsBefore++;
                foundDigits = true;
                ++pos;
            } else if (format[pos] == ',') {
                ++pos; // Skip additional commas
            } else {
                break;
            }
        }
    }
    
    // Check for decimal point
    if (pos < format.length() && format[pos] == '.') {
        pattern.hasDecimal = true;
        ++pos;
        
        // Count digits after decimal point
        while (pos < format.length() && format[pos] == '#') {
            pattern.digitsAfter++;
            foundDigits = true;
            ++pos;
        }
    }
    
    // Check for exponential notation ^^^^
    if (pos + 3 < format.length() && 
        format.substr(pos, 4) == "^^^^") {
        pattern.exponential = true;
        pos += 4;
        foundDigits = true;
    }
    
    // Check for trailing sign
    if (pos < format.length() && format[pos] == '+') {
        pattern.trailingSign = true;
        ++pos;
    }
    
    // Calculate total width
    pattern.totalWidth = static_cast<int>(pos - start);
    
    if (!foundDigits) {
        pos = start; // Reset position if no pattern found
        return std::nullopt;
    }
    
    return pattern;
}

std::optional<StringPattern> NumericEngine::parseStringFormat(const std::string& format, size_t& pos) {
    if (pos >= format.length()) return std::nullopt;
    
    StringPattern pattern;
    
    if (format[pos] == '!') {
        // Single character pattern
        pattern.type = StringPattern::SINGLE_CHAR;
        pattern.width = 1;
        ++pos;
        return pattern;
    }
    
    if (format[pos] == '&') {
        // Variable length pattern
        pattern.type = StringPattern::VARIABLE_LENGTH;
        pattern.width = 0;
        ++pos;
        return pattern;
    }
    
    if (format[pos] == '\\') {
        // Fixed width pattern \   \
        pattern.type = StringPattern::FIXED_WIDTH;
        pattern.width = 2; // Minimum width (just the backslashes)
        ++pos;
        
        // Count spaces between backslashes
        while (pos < format.length() && format[pos] == ' ') {
            pattern.width++;
            ++pos;
        }
        
        if (pos < format.length() && format[pos] == '\\') {
            ++pos;
            return pattern;
        } else {
            // Invalid pattern - backslash not closed
            return std::nullopt;
        }
    }
    
    return std::nullopt;
}

std::string NumericEngine::formatWithNumericPattern(const NumericPattern& pattern, const NumericValue& value) {
    // Convert value to double for formatting
    double val = std::visit([](auto&& x) -> double {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Int16>) return x.v;
        if constexpr (std::is_same_v<T, Single>) return x.v;
        if constexpr (std::is_same_v<T, Double>) return x.v;
        return 0.0;
    }, value);
    
    std::string result;
    bool isNegative = val < 0;
    val = std::abs(val);
    
    if (pattern.exponential) {
        // Exponential format
        std::ostringstream oss;
        oss << std::scientific << std::setprecision(6) << val;
        result = oss.str();
        
        // Convert 'e' to 'E' and ensure proper format
        size_t ePos = result.find('e');
        if (ePos != std::string::npos) {
            result[ePos] = 'E';
        }
    } else {
        // Fixed-point format
        std::ostringstream oss;
        
        if (pattern.hasDecimal && pattern.digitsAfter > 0) {
            oss << std::fixed << std::setprecision(pattern.digitsAfter);
        }
        
        oss << val;
        result = oss.str();
        
        // Add thousands separators if needed
        if (pattern.hasCommas) {
            size_t decimalPos = result.find('.');
            size_t integerEnd = (decimalPos != std::string::npos) ? decimalPos : result.length();
            
            // Insert commas every 3 digits from the right
            for (int i = static_cast<int>(integerEnd) - 3; i > 0; i -= 3) {
                result.insert(i, ",");
            }
        }
    }
    
    // Handle sign formatting
    if (pattern.leadingSign || pattern.trailingSign) {
        char sign = isNegative ? '-' : '+';
        if (pattern.leadingSign) {
            result = sign + result;
        }
        if (pattern.trailingSign) {
            result = result + sign;
        }
    } else if (isNegative) {
        result = "-" + result;
    }
    
    // Handle dollar sign
    if (pattern.floatDollar) {
        result = "$$" + result;
    } else if (pattern.dollarSign) {
        result = "$" + result;
    }
    
    // Pad or truncate to field width
    if (pattern.totalWidth > 0) {
        if (result.length() > static_cast<size_t>(pattern.totalWidth)) {
            // Field overflow - return string with % prefix (GW-BASIC behavior)
            return "%" + result;
        }
        
        // Pad with spaces or asterisks
        char fillChar = pattern.asteriskFill ? '*' : ' ';
        while (result.length() < static_cast<size_t>(pattern.totalWidth)) {
            result = fillChar + result;
        }
    }
    
    return result;
}

std::string NumericEngine::formatWithStringPattern(const StringPattern& pattern, const std::string& value) {
    switch (pattern.type) {
        case StringPattern::SINGLE_CHAR:
            return value.empty() ? " " : std::string(1, value[0]);
            
        case StringPattern::VARIABLE_LENGTH:
            return value;
            
        case StringPattern::FIXED_WIDTH:
            if (value.length() >= static_cast<size_t>(pattern.width)) {
                return value.substr(0, pattern.width);
            } else {
                // Pad with spaces
                std::string result = value;
                while (result.length() < static_cast<size_t>(pattern.width)) {
                    result += " ";
                }
                return result;
            }
    }
    
    return value;
}
