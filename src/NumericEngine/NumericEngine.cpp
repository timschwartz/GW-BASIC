#include "NumericEngine.hpp"

#include <cmath>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <random>
#include <chrono>

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
