#include "ExpressionEvaluator.hpp"

#include <cassert>
#include <cctype>
#include <stdexcept>
#include <cmath>
#include <limits>

#include "../Tokenizer/Tokenizer.hpp"

namespace expr {

static bool isDigitByte(uint8_t b) { return b >= '0' && b <= '9'; }

ExpressionEvaluator::ExpressionEvaluator(std::shared_ptr<Tokenizer> tok)
    : tokenizer(std::move(tok)) {}

EvalResult ExpressionEvaluator::evaluate(const std::vector<uint8_t>& bytes, size_t startPos, Env& env) {
    size_t pos = startPos;
    skipSpaces(bytes, pos);
    auto val = parseExpression(bytes, pos, env, 0);
    skipSpaces(bytes, pos);
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

bool ExpressionEvaluator::isAsciiSpace(uint8_t c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }
bool ExpressionEvaluator::isAsciiDigit(uint8_t c) { return c >= '0' && c <= '9'; }
bool ExpressionEvaluator::isAsciiAlpha(uint8_t c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
bool ExpressionEvaluator::isAsciiAlnum(uint8_t c) { return isAsciiAlpha(c) || isAsciiDigit(c); }

void ExpressionEvaluator::skipSpaces(const std::vector<uint8_t>& b, size_t& pos) {
    while (pos < b.size() && isAsciiSpace(b[pos])) pos++;
}

// Decode Tokenizer numeric constants if present; advances pos on success
bool ExpressionEvaluator::tryDecodeNumber(const std::vector<uint8_t>& b, size_t& pos, Value& out) {
    if (pos >= b.size()) return false;
    uint8_t t = b[pos];
    // 0x11 int16, 0x1D float, 0x1F double
    if (t == 0x11) {
        if (pos + 2 >= b.size()) return false;
        int16_t v = static_cast<int16_t>(static_cast<uint16_t>(b[pos + 1]) | (static_cast<uint16_t>(b[pos + 2]) << 8));
        out = Int16{v};
        pos += 3;
        return true;
    } else if (t == 0x1D) {
        if (pos + 4 >= b.size()) return false;
        union { uint32_t i; float f; } u{};
        u.i = static_cast<uint32_t>(b[pos + 1]) |
              (static_cast<uint32_t>(b[pos + 2]) << 8) |
              (static_cast<uint32_t>(b[pos + 3]) << 16) |
              (static_cast<uint32_t>(b[pos + 4]) << 24);
        out = Single{u.f};
        pos += 5;
        return true;
    } else if (t == 0x1F) {
        if (pos + 8 >= b.size()) return false;
        union { uint64_t i; double d; } u{};
        u.i = 0;
        for (int j = 0; j < 8; ++j) {
            u.i |= (static_cast<uint64_t>(b[pos + 1 + j]) << (j * 8));
        }
        out = Double{u.d};
        pos += 9;
        return true;
    }
    return false;
}

bool ExpressionEvaluator::tryDecodeString(const std::vector<uint8_t>& b, size_t& pos, Value& out) {
    if (pos >= b.size() || b[pos] != '"') return false;
    size_t i = pos + 1;
    std::string s;
    while (i < b.size() && b[i] != 0x00 && b[i] != '"') {
        s.push_back(static_cast<char>(b[i]));
        ++i;
    }
    if (i >= b.size() || b[i] != '"') return false; // unterminated
    out = Str{std::move(s)};
    pos = i + 1;
    return true;
}

std::string ExpressionEvaluator::readIdentifier(const std::vector<uint8_t>& b, size_t& pos) {
    std::string id;
    if (pos < b.size() && isAsciiAlpha(b[pos])) {
        id.push_back(static_cast<char>(b[pos++]));
        while (pos < b.size()) {
            uint8_t c = b[pos];
            if (isAsciiAlnum(c) || c == '$' || c == '%' || c == '!' || c == '#') {
                id.push_back(static_cast<char>(c));
                ++pos;
            } else break;
        }
    }
    return id;
}

ExpressionEvaluator::OpInfo ExpressionEvaluator::peekOperator(const std::vector<uint8_t>& b, size_t pos) const {
    OpInfo none{"", -1, -1, false};
    if (atEnd(b, pos)) return none;
    // Multi-char comparisons first
    if (pos + 1 < b.size()) {
        uint8_t c0 = b[pos], c1 = b[pos + 1];
        if ((c0 == '<' && c1 == '=') || (c0 == '>' && c1 == '=') || (c0 == '<' && c1 == '>')) {
            std::string op; op.push_back(static_cast<char>(c0)); op.push_back(static_cast<char>(c1));
            return {op, 40, 41, false};
        }
    }
    uint8_t c = b[pos];
    switch (c) {
        case '^': return {"^", 80, 79, true};
        case '*': return {"*", 60, 61, false};
        case '/': return {"/", 60, 61, false};
        case '\\': return {"\\", 60, 61, false};
        // MOD as word handled elsewhere
        case '+': return {"+", 50, 51, false};
        case '-': return {"-", 50, 51, false};
        case '=': return {"=", 40, 41, false};
        case '<': return {"<", 40, 41, false};
        case '>': return {">", 40, 41, false};
        default: break;
    }
    // Word operators: AND/OR/XOR/EQV/IMP/MOD/NOT
    size_t p = pos;
    std::string w;
    while (p < b.size() && isAsciiAlpha(b[p])) { w.push_back(static_cast<char>(std::toupper(b[p]))); p++; }
    if (w == "AND") return {"AND", 30, 31, false};
    if (w == "OR") return {"OR", 20, 21, false};
    if (w == "XOR") return {"XOR", 20, 21, false};
    if (w == "EQV") return {"EQV", 10, 11, false};
    if (w == "IMP") return {"IMP", 10, 11, false};
    if (w == "MOD") return {"MOD", 60, 61, false};
    return none;
}

bool ExpressionEvaluator::isComparison(const std::string& op) {
    return op == "=" || op == "<>" || op == "<" || op == ">" || op == "<=" || op == ">=";
}

bool ExpressionEvaluator::isNumeric(const Value& v) {
    return std::holds_alternative<Int16>(v) || std::holds_alternative<Single>(v) || std::holds_alternative<Double>(v);
}

double ExpressionEvaluator::toDouble(const Value& v) {
    return std::visit([](auto&& x) -> double {
        using T = std::decay_t<decltype(x)>;
        if constexpr (std::is_same_v<T, Int16>) return static_cast<double>(x.v);
        if constexpr (std::is_same_v<T, Single>) return static_cast<double>(x.v);
        if constexpr (std::is_same_v<T, Double>) return x.v;
        if constexpr (std::is_same_v<T, Str>) throw BasicError(13, "Type mismatch", 0);
    }, v);
}

int16_t ExpressionEvaluator::toInt16(const Value& v) {
    if (std::holds_alternative<Int16>(v)) return std::get<Int16>(v).v;
    double d = toDouble(v);
    if (d > std::numeric_limits<int16_t>::max()) return std::numeric_limits<int16_t>::max();
    if (d < std::numeric_limits<int16_t>::min()) return std::numeric_limits<int16_t>::min();
    return static_cast<int16_t>(d);
}

Value ExpressionEvaluator::makeNumericResult(double a, double b, double result, bool bothInt, bool forceInt) {
    if (forceInt) {
        // Integer division and logical ops return Int16
        return Int16{static_cast<int16_t>(result)};
    }
    if (bothInt && std::floor(result) == result && result >= std::numeric_limits<int16_t>::min() && result <= std::numeric_limits<int16_t>::max()) {
        return Int16{static_cast<int16_t>(result)};
    }
    // If any operand was floating, prefer double for safety
    return Double{result};
}

Value ExpressionEvaluator::parsePrimary(const std::vector<uint8_t>& b, size_t& pos, Env& env) {
    if (atEnd(b, pos)) throw BasicError(2, "Syntax error", pos);
    uint8_t t = b[pos];

    // Numeric literal markers (tokenized)
    Value num;
    if (tryDecodeNumber(b, pos, num)) return num;

    // String literal (tokenized/ASCII quotes)
    Value str;
    if (tryDecodeString(b, pos, str)) return str;

    // Unary operators: +, -, NOT
    if (t == '+' || t == '-') {
        char op = static_cast<char>(t);
        ++pos; skipSpaces(b, pos);
        // prefix power lower than '^' to ensure -5^2 == -(5^2)
        auto rhs = parseExpression(b, pos, env, 60);
        if (op == '+') return rhs;
        // Negation
        if (!isNumeric(rhs)) throw BasicError(13, "Type mismatch", pos);
        return Double{-toDouble(rhs)};
    }
    // NOT prefix
    if ((t == 'N' || t == 'n') && pos + 2 < b.size() && (std::toupper(b[pos]) == 'N') && (std::toupper(b[pos+1]) == 'O') && (std::toupper(b[pos+2]) == 'T')) {
        pos += 3; skipSpaces(b, pos);
        auto rhs = parseExpression(b, pos, env, 70);
        int16_t bint = toBoolInt(rhs);
        return Int16{static_cast<int16_t>(~bint)}; // In BASIC, NOT flips all bits; for booleans, ~(-1)=0, ~(0)=-1
    }

    // ASCII integer literal
    if (isDigitByte(t)) {
        int value = 0;
        while (!atEnd(b, pos) && isAsciiDigit(b[pos])) {
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

    // Identifier -> variable
    if (isAsciiAlpha(t)) {
        auto id = readIdentifier(b, pos);
        // Try external resolver first
        if (env.getVar) {
            Value ext{};
            if (env.getVar(id, ext)) { return ext; }
        }
        auto it = env.vars.find(id);
        if (it != env.vars.end()) return it->second;
        throw BasicError(2, "Undefined variable: " + id, pos);
    }

    // Fallback: treat anything else as syntax error for now
    throw BasicError(2, "Syntax error", pos);
}

// Pratt parser with basic operators
Value ExpressionEvaluator::parseExpression(const std::vector<uint8_t>& b, size_t& pos, Env& env, int minBp) {
    auto lhs = parsePrimary(b, pos, env);
    skipSpaces(b, pos);

    while (!atEnd(b, pos)) {
        // Word operators require separate lookahead without consuming spaces
        OpInfo op = peekOperator(b, pos);
        if (op.lbp < minBp) break;

        // consume operator
        if (op.op == "<>" || op.op == "<=" || op.op == ">=") {
            pos += 2;
        } else if (op.op == "AND" || op.op == "OR" || op.op == "XOR" || op.op == "EQV" || op.op == "IMP" || op.op == "MOD") {
            pos += op.op.size();
        } else {
            pos += 1;
        }
        skipSpaces(b, pos);

        int nextMin = op.rightAssoc ? op.rbp : (op.lbp + 1);
        auto rhs = parseExpression(b, pos, env, nextMin);

        // Apply operator
        if (isComparison(op.op)) {
            double a = toDouble(lhs);
            double c = toDouble(rhs);
            bool res = false;
            if (op.op == "=") res = (a == c);
            else if (op.op == "<>") res = (a != c);
            else if (op.op == "<") res = (a < c);
            else if (op.op == ">") res = (a > c);
            else if (op.op == "<=") res = (a <= c);
            else if (op.op == ">=") res = (a >= c);
            lhs = Int16{static_cast<int16_t>(res ? -1 : 0)};
            skipSpaces(b, pos);
            continue;
        }

        if (op.op == "+" || op.op == "-" || op.op == "*" || op.op == "/" || op.op == "^" || op.op == "\\" || op.op == "MOD") {
            if (!isNumeric(lhs) || !isNumeric(rhs)) throw BasicError(13, "Type mismatch", pos);
            double a = toDouble(lhs);
            double c = toDouble(rhs);
            bool bothInt = std::holds_alternative<Int16>(lhs) && std::holds_alternative<Int16>(rhs);
            if (op.op == "+") lhs = makeNumericResult(a, c, a + c, bothInt, false);
            else if (op.op == "-") lhs = makeNumericResult(a, c, a - c, bothInt, false);
            else if (op.op == "*") lhs = makeNumericResult(a, c, a * c, bothInt, false);
            else if (op.op == "/") { if (c == 0.0) throw BasicError(11, "Division by zero", pos); lhs = Double{a / c}; }
            else if (op.op == "^") lhs = Double{std::pow(a, c)};
            else if (op.op == "\\") { if (c == 0.0) throw BasicError(11, "Division by zero", pos); lhs = Int16{static_cast<int16_t>(static_cast<long long>(std::floor(a / c)))}; }
            else if (op.op == "MOD") { if (static_cast<long long>(c) == 0ll) throw BasicError(11, "Division by zero", pos); lhs = Int16{static_cast<int16_t>(static_cast<long long>(a) % static_cast<long long>(c))}; }
            skipSpaces(b, pos);
            continue;
        }

        // Logical ops (AND/OR/XOR/EQV/IMP) on Int16 truth values
        int16_t la = toBoolInt(lhs);
        int16_t rb = toBoolInt(rhs);
        if (op.op == "AND") lhs = Int16{static_cast<int16_t>(la & rb)};
        else if (op.op == "OR") lhs = Int16{static_cast<int16_t>(la | rb)};
        else if (op.op == "XOR") lhs = Int16{static_cast<int16_t>(la ^ rb)};
        else if (op.op == "EQV") lhs = Int16{static_cast<int16_t>(~(la ^ rb))};
        else if (op.op == "IMP") lhs = Int16{static_cast<int16_t>((~la) | rb)};
        else throw BasicError(2, "Syntax error", pos);
        skipSpaces(b, pos);
    }

    return lhs;
}

} // namespace expr
