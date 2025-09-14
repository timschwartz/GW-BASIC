// String function integration layer for GW-BASIC runtime
// Provides proper StringManager integration for string functions
#pragma once

#include <memory>
#include <string>
#include <vector>
#include <variant>
#include "StringManager.hpp"
#include "Value.hpp"

namespace gwbasic {

// Forward declaration for expression evaluation values
namespace expr {
    struct Int16 { int16_t v{}; };
    struct Single { float v{}; };
    struct Double { double v{}; };
    struct Str { std::string v; };
    using Value = std::variant<Int16, Single, Double, Str>;
}

/**
 * StringFunctionProcessor provides integrated string function support
 * that properly uses StringManager for memory management and interoperates
 * with both expr::Value (from ExpressionEvaluator) and gwbasic::Value types.
 */
class StringFunctionProcessor {
public:
    explicit StringFunctionProcessor(std::shared_ptr<StringManager> stringManager)
        : stringManager_(std::move(stringManager)) {}

    // Convert between expression evaluator values and runtime values
    gwbasic::Value exprToRuntime(const expr::Value& exprVal);
    expr::Value runtimeToExpr(const gwbasic::Value& runtimeVal);

    // String function implementations using StringManager
    gwbasic::Value chr(int16_t asciiCode);
    gwbasic::Value str(const gwbasic::Value& numericValue);
    gwbasic::Value left(const gwbasic::Value& source, int16_t count);
    gwbasic::Value right(const gwbasic::Value& source, int16_t count);
    gwbasic::Value mid(const gwbasic::Value& source, int16_t start, int16_t optCount = -1);
    gwbasic::Value val(const gwbasic::Value& stringValue);
    gwbasic::Value string(int16_t count, const gwbasic::Value& charOrAscii);
    gwbasic::Value space(int16_t count);
    int16_t len(const gwbasic::Value& stringValue);
    int16_t asc(const gwbasic::Value& stringValue);
    int16_t instr(const gwbasic::Value& source, const gwbasic::Value& search, int16_t start = 1);

    // Function call interface for ExpressionEvaluator integration
    bool callStringFunction(const std::string& funcName, const std::vector<expr::Value>& args, expr::Value& result);

    // Get access to underlying StringManager for other components
    std::shared_ptr<StringManager> getStringManager() const { return stringManager_; }

private:
    std::shared_ptr<StringManager> stringManager_;

    // Helper to ensure string value
    const StrDesc& ensureString(const gwbasic::Value& val);
    
    // Helper to convert numeric value to string representation  
    std::string numericToString(const gwbasic::Value& val);
    
    // Helper to parse string as numeric value
    gwbasic::Value parseNumeric(const std::string& str);
    
    // Utility functions
    static bool isNumeric(const gwbasic::Value& val);
    static bool isString(const gwbasic::Value& val);
    static double toDouble(const gwbasic::Value& val);
    static int16_t toInt16(const gwbasic::Value& val);
};

} // namespace gwbasic