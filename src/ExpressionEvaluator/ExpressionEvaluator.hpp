#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>
#include <functional>

class Tokenizer;

// Forward declarations for integration
class NumericEngine;

namespace expr {

// Basic runtime value types for GW-BASIC
struct Int16 { int16_t v{}; };
struct Single { float v{}; };
struct Double { double v{}; };
struct Str { std::string v; };

using Value = std::variant<Int16, Single, Double, Str>;

// Evaluation environment (variables, options, built-ins later)
struct Env {
    int optionBase{0};
    // Optional: external variable resolver; if set, used before local map.
    std::function<bool(const std::string& name, Value& out)> getVar;
    std::unordered_map<std::string, Value> vars; // fallback/local storage
    
    // Optional: external function resolver for built-in functions
    std::function<bool(const std::string& name, const std::vector<Value>& args, Value& out)> callFunc;
    
    // Optional: array element resolver for subscripted variables
    std::function<bool(const std::string& name, const std::vector<Value>& indices, Value& out)> getArrayElem;
    
    // Optional: numeric engine for math functions
    std::shared_ptr<NumericEngine> numericEngine;
};

// Evaluation result with next token position
struct EvalResult {
    Value value;
    size_t nextPos{0};
};

// Exception type for BASIC errors
struct BasicError : public std::runtime_error {
    int code; // keep for mapping later
    size_t position;
    BasicError(int c, const std::string& m, size_t p)
        : std::runtime_error(m), code(c), position(p) {}
};

class ExpressionEvaluator {
public:
    explicit ExpressionEvaluator(std::shared_ptr<Tokenizer> tok);

    // Evaluate an expression from token bytes[startPos:)
    EvalResult evaluate(const std::vector<uint8_t>& bytes, size_t startPos, Env& env);

    // Truthiness: 0 -> 0, nonzero -> -1 (GW-BASIC boolean)
    static int16_t toBoolInt(const Value& v);

    // Numeric conversions for integration points
    static double toDouble(const Value& v);
    static int16_t toInt16(const Value& v);

private:
    std::shared_ptr<Tokenizer> tokenizer;

    // Pratt parser entry
    Value parseExpression(const std::vector<uint8_t>& b, size_t& pos, Env& env, int minBp = 0);

    // Primaries and helpers
    Value parsePrimary(const std::vector<uint8_t>& b, size_t& pos, Env& env);
    Value parseFunction(const std::string& funcName, const std::vector<uint8_t>& b, size_t& pos, Env& env);
    Value parseBuiltinFunction(const std::string& funcName, const std::vector<Value>& args, Env& env);
    std::vector<Value> parseArgumentList(const std::vector<uint8_t>& b, size_t& pos, Env& env);
    bool atEnd(const std::vector<uint8_t>& b, size_t pos) const;

    // Array element access
    Value parseArrayAccess(const std::string& arrayName, const std::vector<uint8_t>& b, size_t& pos, Env& env);
    bool isKnownFunction(const std::string& name) const;

    // Utilities
    static void skipSpaces(const std::vector<uint8_t>& b, size_t& pos);
    static bool isAsciiSpace(uint8_t c);
    static bool isAsciiDigit(uint8_t c);
    static bool isAsciiAlpha(uint8_t c);
    static bool isAsciiAlnum(uint8_t c);

    // Token decoding helpers for Tokenizer-encoded numbers/strings
    static bool tryDecodeNumber(const std::vector<uint8_t>& b, size_t& pos, Value& out);
    static bool tryDecodeString(const std::vector<uint8_t>& b, size_t& pos, Value& out);
    static bool tryDecodeFunction(const std::vector<uint8_t>& b, size_t pos, std::string& funcName);
    static std::string readIdentifier(const std::vector<uint8_t>& b, size_t& pos);

    // Operator handling
    struct OpInfo { std::string op; int lbp; int rbp; bool rightAssoc; };
    OpInfo peekOperator(const std::vector<uint8_t>& b, size_t pos) const;
    static bool isComparison(const std::string& op);

    // Arithmetic helpers
    static bool isNumeric(const Value& v);
    static Value makeNumericResult(double a, double b, double result, bool bothInt, bool forceInt);
};

} // namespace expr
