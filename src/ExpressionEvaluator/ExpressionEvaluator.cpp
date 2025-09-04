#include "ExpressionEvaluator.hpp"

#include <cassert>
#include <cctype>
#include <stdexcept>

#include "../Tokenizer/Tokenizer.hpp"

namespace expr {

static bool isDigitByte(uint8_t b) { return b >= '0' && b <= '9'; }

ExpressionEvaluator::ExpressionEvaluator(std::shared_ptr<Tokenizer> tok)
    : tokenizer(std::move(tok)) {}

EvalResult ExpressionEvaluator::evaluate(const std::vector<uint8_t>& bytes, size_t startPos, Env& env) {
    size_t pos = startPos;
    auto val = parseExpression(bytes, pos, env, 0);
    return EvalResult{std::move(val), pos};
}

int16_t ExpressionEvaluator::toBoolInt(const Value& v) {
    auto nonzero = std::visit([](auto&& x) -> bool {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Int16>) return x.v != 0;
        if constexpr (std::is_same_v<T, Single>) return x.v != 0.0f;
        if constexpr (std::is_same_v<T, Double>) return x.v != 0.0;
        if constexpr (std::is_same_v<T, Str>) return !x.v.empty();
    }, v);
    return nonzero ? int16_t(-1) : int16_t(0);
}

bool ExpressionEvaluator::atEnd(const std::vector<uint8_t>& b, size_t pos) const {
    return pos >= b.size() || b[pos] == 0x00; // LINE_TERMINATOR
}

Value ExpressionEvaluator::parsePrimary(const std::vector<uint8_t>& b, size_t& pos, Env& env) {
    if (atEnd(b, pos)) throw BasicError(2, "Syntax error", pos);
    uint8_t t = b[pos];

    // Very minimal: integer literal as ASCII digits (temporary until we decode MBF literals)
    if (isDigitByte(t)) {
        int value = 0;
        while (!atEnd(b, pos) && isDigitByte(b[pos])) {
            value = value * 10 + (b[pos] - '0');
            ++pos;
        }
        return Int16{static_cast<int16_t>(value)};
    }

    // Parenthesized expression
    if (t == '(') {
        ++pos; // consume '('
        auto inner = parseExpression(b, pos, env, 0);
        if (atEnd(b, pos) || b[pos] != ')') throw BasicError(2, "Syntax error: missing )", pos);
        ++pos; // consume ')'
        return inner;
    }

    // Fallback: treat anything else as syntax error for now
    throw BasicError(2, "Syntax error", pos);
}

// Pratt parser skeleton: only primary for now
Value ExpressionEvaluator::parseExpression(const std::vector<uint8_t>& b, size_t& pos, Env& env, int /*minBp*/) {
    // For now, just parse a primary
    return parsePrimary(b, pos, env);
}

} // namespace expr
