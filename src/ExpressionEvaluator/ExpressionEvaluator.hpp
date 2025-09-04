#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

class Tokenizer;

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
    std::unordered_map<std::string, Value> vars;
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

private:
    std::shared_ptr<Tokenizer> tokenizer;

    // Pratt parser entry
    Value parseExpression(const std::vector<uint8_t>& b, size_t& pos, Env& env, int minBp = 0);

    // Primaries and helpers
    Value parsePrimary(const std::vector<uint8_t>& b, size_t& pos, Env& env);
    bool atEnd(const std::vector<uint8_t>& b, size_t pos) const;
};

} // namespace expr
