#include "ExpressionEvaluator.hpp"

#include <cassert>
#include <cctype>
#include <stdexcept>
#include <cmath>
#include <limits>
#include <algorithm>
#include <sstream>
#include <random>
#include <unordered_set>
#include <iostream>

#include "../Tokenizer/Tokenizer.hpp"
#include "../NumericEngine/NumericEngine.hpp"

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
    // End if we've run past the buffer
    if (pos >= b.size()) return true;
    // Simple check: only treat isolated 0x00 as line terminator
    if (b[pos] == 0x00) return true;
    return false;
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

bool ExpressionEvaluator::tryDecodeFunction(const std::vector<uint8_t>& b, size_t pos, std::string& funcName) {
    if (pos >= b.size()) return false;
    
    // Check for two-byte function token (0xFF followed by function code)
    if (b[pos] == 0xFF && pos + 1 < b.size()) {
        uint8_t funcCode = b[pos + 1];
        // Map function codes to names (based on Tokenizer.cpp)
        switch (funcCode) {
            case 0x00: funcName = "LEFT$"; return true;
            case 0x01: funcName = "RIGHT$"; return true;
            case 0x02: funcName = "MID$"; return true;
            case 0x03: funcName = "SGN"; return true;
            case 0x04: funcName = "INT"; return true;
            case 0x05: funcName = "ABS"; return true;
            case 0x06: funcName = "SQR"; return true;
            case 0x07: funcName = "RND"; return true;
            case 0x08: funcName = "SIN"; return true;
            case 0x09: funcName = "LOG"; return true;
            case 0x0A: funcName = "EXP"; return true;
            case 0x0B: funcName = "COS"; return true;
            case 0x0C: funcName = "TAN"; return true;
            case 0x0D: funcName = "ATN"; return true;
            case 0x0E: funcName = "FRE"; return true;
            case 0x0F: funcName = "INP"; return true;
            case 0x10: funcName = "POS"; return true;
            case 0x11: funcName = "LEN"; return true;
            case 0x12: funcName = "STR$"; return true;
            case 0x13: funcName = "VAL"; return true;
            case 0x14: funcName = "ASC"; return true;
            case 0x15: funcName = "CHR$"; return true;
            case 0x16: funcName = "PEEK"; return true;
            case 0x17: funcName = "SPACE$"; return true;
            case 0x18: funcName = "STRING$"; return true;
            case 0x19: funcName = "OCT$"; return true;
            case 0x1A: funcName = "HEX$"; return true;
            case 0x1B: funcName = "LPOS"; return true;
            case 0x1C: funcName = "CINT"; return true;
            case 0x1D: funcName = "CSNG"; return true;
            case 0x1E: funcName = "CDBL"; return true;
            case 0x1F: funcName = "FIX"; return true;
            case 0x20: funcName = "PEN"; return true;
            case 0x21: funcName = "STICK"; return true;
            case 0x22: funcName = "STRIG"; return true;
            case 0x23: funcName = "EOF"; return true;
            case 0x24: funcName = "LOC"; return true;
            case 0x25: funcName = "LOF"; return true;
            case 0x26: funcName = "INKEY$"; return true;
            default: return false;
        }
    }
    
    return false;
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

std::vector<Value> ExpressionEvaluator::parseArgumentList(const std::vector<uint8_t>& b, size_t& pos, Env& env) {
    std::vector<Value> args;

    // Expect opening parenthesis or square bracket
    skipSpaces(b, pos);

    // Use tokenizer-aware detection for parentheses
    auto isOpenParen = [&](uint8_t ch) { 
        if (ch == '(' || ch == '[') return true;
        if (ch >= 0x80 && tokenizer) {
            std::string tokenName = tokenizer->getTokenName(ch);
            return tokenName == "(" || tokenName == "[";
        }
        return false;
    };
    auto isCloseParen = [&](uint8_t expectedClose, uint8_t ch) {
        // ASCII close
        if (ch == expectedClose) return true;
        // Tokenized close
        if (ch >= 0x80 && tokenizer) {
            std::string tokenName = tokenizer->getTokenName(ch);
            if (expectedClose == ')' && tokenName == ")") return true;
            if (expectedClose == ']' && tokenName == "]") return true;
        }
        return false;
    };

    if (atEnd(b, pos) || !isOpenParen(b[pos])) {
        return args; // No arguments
    }

    // Determine closing delimiter
    uint8_t closeChar;
    if (b[pos] == '[') closeChar = ']';
    else if (b[pos] >= 0x80 && tokenizer && tokenizer->getTokenName(b[pos]) == "[") closeChar = ']';
    else closeChar = ')'; // for '(' or tokenized '('

    // consume opening character (ASCII '(' or '[') or tokenized '('
    ++pos;
    skipSpaces(b, pos);

    // Handle empty argument list
    if (!atEnd(b, pos) && isCloseParen(closeChar, b[pos])) {
        ++pos; // consume closing character
        return args;
    }

    // Parse arguments separated by commas
    while (!atEnd(b, pos)) {
        skipSpaces(b, pos);
        if (atEnd(b, pos)) {
            throw BasicError(2, "Syntax error: missing closing " + std::string(1, static_cast<char>(closeChar)), pos);
        }

        // Check for closing character before parsing argument
        if (isCloseParen(closeChar, b[pos])) {
            ++pos; // consume closing character
            break;
        }

        auto arg = parseExpression(b, pos, env, 0);
        args.push_back(std::move(arg));

        skipSpaces(b, pos);
        if (atEnd(b, pos)) {
            throw BasicError(2, "Syntax error: missing closing " + std::string(1, static_cast<char>(closeChar)), pos);
        }

        // Use tokenizer-aware comma detection
        if (isCloseParen(closeChar, b[pos])) {
            ++pos; // consume closing character
            break;
        } else if (b[pos] == ',' || (b[pos] >= 0x80 && tokenizer && tokenizer->getTokenName(b[pos]) == ",")) {
            ++pos; // consume ','
            skipSpaces(b, pos);
        } else {
            throw BasicError(2, "Syntax error: expected ',' or '" + std::string(1, static_cast<char>(closeChar)) + "'", pos);
        }
    }

    return args;
}

Value ExpressionEvaluator::parseBuiltinFunction(const std::string& funcName, const std::vector<Value>& args, Env& env) {
    // Convert function name to uppercase for comparison
    std::string upperFuncName = funcName;
    std::transform(upperFuncName.begin(), upperFuncName.end(), upperFuncName.begin(), 
                   [](unsigned char c) { return std::toupper(c); });
    
    // Math functions
    if (upperFuncName == "ABS" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Double{std::abs(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "SGN" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            int16_t result = (val > 0) ? 1 : (val < 0) ? -1 : 0;
            return Int16{result};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "INT" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Int16{static_cast<int16_t>(std::floor(val))};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "FIX" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Int16{static_cast<int16_t>(std::trunc(val))};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "SQR" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            if (val < 0) throw BasicError(5, "Illegal function call", 0);
            return Double{std::sqrt(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "SIN" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Double{std::sin(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "COS" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Double{std::cos(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "TAN" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Double{std::tan(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "ATN" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Double{std::atan(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "LOG" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            if (val <= 0) throw BasicError(5, "Illegal function call", 0);
            return Double{std::log(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "EXP" && args.size() == 1) {
        if (isNumeric(args[0])) {
            double val = toDouble(args[0]);
            return Double{std::exp(val)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "RND") {
        // Simple fallback RND implementation
        static std::mt19937 gen(std::random_device{}());
        std::uniform_real_distribution<float> dist(0.0f, 1.0f);
        return Single{dist(gen)};
    }
    
    // String functions (basic implementations)
    if (upperFuncName == "LEN" && args.size() == 1) {
        if (std::holds_alternative<Str>(args[0])) {
            return Int16{static_cast<int16_t>(std::get<Str>(args[0]).v.length())};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "ASC" && args.size() == 1) {
        if (std::holds_alternative<Str>(args[0])) {
            const auto& str = std::get<Str>(args[0]).v;
            if (str.empty()) throw BasicError(5, "Illegal function call", 0);
            return Int16{static_cast<int16_t>(static_cast<unsigned char>(str[0]))};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "CHR$" && args.size() == 1) {
        if (isNumeric(args[0])) {
            int16_t code = toInt16(args[0]);
            if (code < 0 || code > 255) throw BasicError(5, "Illegal function call", 0);
            return Str{std::string(1, static_cast<char>(code))};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "STR$" && args.size() == 1) {
        if (isNumeric(args[0])) {
            std::ostringstream oss;
            std::visit([&oss](auto&& arg) {
                using T = std::decay_t<decltype(arg)>;
                if constexpr (std::is_same_v<T, Int16>) oss << arg.v;
                if constexpr (std::is_same_v<T, Single>) oss << arg.v;
                if constexpr (std::is_same_v<T, Double>) oss << arg.v;
            }, args[0]);
            std::string result = oss.str();
            if (result[0] != '-') result = " " + result; // Add leading space for positive numbers
            return Str{result};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "VAL" && args.size() == 1) {
        if (std::holds_alternative<Str>(args[0])) {
            const auto& str = std::get<Str>(args[0]).v;
            try {
                // Try parsing as integer first
                if (str.find('.') == std::string::npos && str.find('e') == std::string::npos && str.find('E') == std::string::npos) {
                    long val = std::stol(str);
                    if (val >= std::numeric_limits<int16_t>::min() && val <= std::numeric_limits<int16_t>::max()) {
                        return Int16{static_cast<int16_t>(val)};
                    }
                }
                // Parse as double
                double val = std::stod(str);
                return Double{val};
            } catch (...) {
                return Int16{0}; // VAL returns 0 for invalid strings
            }
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "LEFT$" && args.size() == 2) {
        if (std::holds_alternative<Str>(args[0]) && isNumeric(args[1])) {
            const auto& str = std::get<Str>(args[0]).v;
            int16_t len = toInt16(args[1]);
            if (len < 0) throw BasicError(5, "Illegal function call", 0);
            if (len >= static_cast<int16_t>(str.length())) return args[0];
            return Str{str.substr(0, len)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "RIGHT$" && args.size() == 2) {
        if (std::holds_alternative<Str>(args[0]) && isNumeric(args[1])) {
            const auto& str = std::get<Str>(args[0]).v;
            int16_t len = toInt16(args[1]);
            if (len < 0) throw BasicError(5, "Illegal function call", 0);
            if (len >= static_cast<int16_t>(str.length())) return args[0];
            return Str{str.substr(str.length() - len)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "MID$" && (args.size() == 2 || args.size() == 3)) {
        if (std::holds_alternative<Str>(args[0]) && isNumeric(args[1])) {
            const auto& str = std::get<Str>(args[0]).v;
            int16_t start = toInt16(args[1]) - 1; // GW-BASIC uses 1-based indexing
            if (start < 0) throw BasicError(5, "Illegal function call", 0);
            if (start >= static_cast<int16_t>(str.length())) return Str{""};
            
            if (args.size() == 3) {
                if (isNumeric(args[2])) {
                    int16_t len = toInt16(args[2]);
                    if (len < 0) throw BasicError(5, "Illegal function call", 0);
                    return Str{str.substr(start, len)};
                }
                throw BasicError(13, "Type mismatch", 0);
            } else {
                return Str{str.substr(start)};
            }
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "STRING$" && args.size() == 2) {
        if (isNumeric(args[0])) {
            int16_t count = toInt16(args[0]);
            if (count < 0) throw BasicError(5, "Illegal function call", 0);
            if (count > 255) throw BasicError(5, "Illegal function call", 0);
            
            char ch;
            if (std::holds_alternative<Str>(args[1])) {
                const auto& str = std::get<Str>(args[1]).v;
                if (str.empty()) throw BasicError(5, "Illegal function call", 0);
                ch = str[0];
            } else if (isNumeric(args[1])) {
                int16_t asciiCode = toInt16(args[1]);
                if (asciiCode < 0 || asciiCode > 255) throw BasicError(5, "Illegal function call", 0);
                ch = static_cast<char>(asciiCode);
            } else {
                throw BasicError(13, "Type mismatch", 0);
            }
            
            return Str{std::string(count, ch)};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "SPACE$" && args.size() == 1) {
        if (isNumeric(args[0])) {
            int16_t count = toInt16(args[0]);
            if (count < 0) throw BasicError(5, "Illegal function call", 0);
            if (count > 255) throw BasicError(5, "Illegal function call", 0);
            return Str{std::string(count, ' ')};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    // Type conversion functions
    if (upperFuncName == "CINT" && args.size() == 1) {
        if (isNumeric(args[0])) {
            return Int16{toInt16(args[0])};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "CSNG" && args.size() == 1) {
        if (isNumeric(args[0])) {
            return Single{static_cast<float>(toDouble(args[0]))};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    if (upperFuncName == "CDBL" && args.size() == 1) {
        if (isNumeric(args[0])) {
            return Double{toDouble(args[0])};
        }
        throw BasicError(13, "Type mismatch", 0);
    }
    
    throw BasicError(2, "Unknown function: " + funcName, 0);
}

Value ExpressionEvaluator::parseFunction(const std::string& funcName, const std::vector<uint8_t>& b, size_t& pos, Env& env) {
    // Parse argument list
    auto args = parseArgumentList(b, pos, env);
    
    // Try external function resolver first
    if (env.callFunc) {
        Value result;
        if (env.callFunc(funcName, args, result)) {
            return result;
        }
    }
    
    // Try built-in functions
    return parseBuiltinFunction(funcName, args, env);
}

ExpressionEvaluator::OpInfo ExpressionEvaluator::peekOperator(const std::vector<uint8_t>& b, size_t pos) const {
    OpInfo none{"", -1, -1, false};
    if (atEnd(b, pos)) return none;
    
    uint8_t c = b[pos];
    
    // Check for tokenized operators first (if we have a tokenizer)
    if (c >= 0x80 && tokenizer) {
        std::string tokenName = tokenizer->getTokenName(c);
        if (tokenName == ">") return {">", 40, 41, false};
        if (tokenName == "=") return {"=", 40, 41, false};
        if (tokenName == "<") return {"<", 40, 41, false};
        if (tokenName == "<>") return {"<>", 40, 41, false};
        if (tokenName == "<=") return {"<=", 40, 41, false};
        if (tokenName == ">=") return {">=", 40, 41, false};
        if (tokenName == "+") return {"+", 50, 51, false};
        if (tokenName == "-") return {"-", 50, 51, false};
        if (tokenName == "*") return {"*", 60, 61, false};
        if (tokenName == "/") return {"/", 60, 61, false};
        if (tokenName == "^") return {"^", 80, 79, true};
        if (tokenName == "\\") return {"\\", 60, 61, false};
        if (tokenName == "AND") return {"AND", 30, 31, false};
        if (tokenName == "OR") return {"OR", 20, 21, false};
        if (tokenName == "XOR") return {"XOR", 20, 21, false};
        if (tokenName == "EQV") return {"EQV", 10, 11, false};
        if (tokenName == "IMP") return {"IMP", 10, 11, false};
        if (tokenName == "MOD") return {"MOD", 60, 61, false};
    }
    
    // Multi-char comparisons first
    if (pos + 1 < b.size()) {
        uint8_t c0 = b[pos], c1 = b[pos + 1];
        if ((c0 == '<' && c1 == '=') || (c0 == '>' && c1 == '=') || (c0 == '<' && c1 == '>')) {
            std::string op; op.push_back(static_cast<char>(c0)); op.push_back(static_cast<char>(c1));
            return {op, 40, 41, false};
        }
    }
    
    // ASCII operators
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

    // Unary operators: +, -, NOT (both ASCII and tokenized forms)
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
    
    // Tokenized unary operators (using tokenizer to resolve token names)
    if (t >= 0x80 && tokenizer) {
        std::string tokenName = tokenizer->getTokenName(t);
        if (tokenName == "+" || tokenName == "-") {
            char op = tokenName[0];
            ++pos; skipSpaces(b, pos);
            // prefix power lower than '^' to ensure -5^2 == -(5^2)
            auto rhs = parseExpression(b, pos, env, 60);
            if (op == '+') return rhs;
            // Negation
            if (!isNumeric(rhs)) throw BasicError(13, "Type mismatch", pos);
            return Double{-toDouble(rhs)};
        }
    }
    // NOT prefix
    if ((t == 'N' || t == 'n') && pos + 2 < b.size() && (std::toupper(b[pos]) == 'N') && (std::toupper(b[pos+1]) == 'O') && (std::toupper(b[pos+2]) == 'T')) {
        pos += 3; skipSpaces(b, pos);
        auto rhs = parseExpression(b, pos, env, 70);
        int16_t bint = toBoolInt(rhs);
        return Int16{static_cast<int16_t>(~bint)}; // In BASIC, NOT flips all bits; for booleans, ~(-1)=0, ~(0)=-1
    }

    // ASCII integer/floating point literal
    if (isDigitByte(t)) {
        double value = 0.0;
        bool isFloat = false;
        
        // Parse integer part
        while (!atEnd(b, pos) && isAsciiDigit(b[pos])) {
            value = value * 10.0 + (b[pos] - '0');
            ++pos;
        }
        
        // Check for decimal point
        if (!atEnd(b, pos) && b[pos] == '.') {
            isFloat = true;
            ++pos; // consume '.'
            double fraction = 0.0;
            double divisor = 10.0;
            
            // Parse fractional part
            while (!atEnd(b, pos) && isAsciiDigit(b[pos])) {
                fraction += (b[pos] - '0') / divisor;
                divisor *= 10.0;
                ++pos;
            }
            value += fraction;
        }
        
        if (isFloat) {
            return Double{value};
        } else {
            return Int16{static_cast<int16_t>(value)};
        }
    }

    // Parenthesized expression (ASCII or tokenized)
    if (t == '(' || (t >= 0x80 && tokenizer && tokenizer->getTokenName(t) == "(")) {
        ++pos; // consume '('
        auto inner = parseExpression(b, pos, env, 0);
        // Check for closing paren (ASCII or tokenized)
        if (atEnd(b, pos) || !(b[pos] == ')' || (b[pos] >= 0x80 && tokenizer && tokenizer->getTokenName(b[pos]) == ")"))) {
            throw BasicError(2, "Syntax error: missing )", pos);
        }
        ++pos; // consume ')'
        return inner;
    }

    // Check for tokenized function call
    std::string funcName;
    if (tryDecodeFunction(b, pos, funcName)) {
        pos += 2; // consume function token (0xFF + function code)
        return parseFunction(funcName, b, pos, env);
    }

    // Check for FN calls (user-defined functions): FN FUNCNAME(args)
    if ((t >= 0x80 && tokenizer && tokenizer->getTokenName(t) == "FN") ||
        (t == 'F' && pos + 1 < b.size() && (b[pos + 1] == 'N' || b[pos + 1] == 'n'))) {
        
        // Skip FN token/word
        if (t >= 0x80) {
            ++pos; // tokenized FN
        } else {
            pos += 2; // ASCII "FN"
        }
        skipSpaces(b, pos);
        
        // Read function name
        if (atEnd(b, pos) || !isAsciiAlpha(b[pos])) {
            throw BasicError(2, "Syntax error: expected function name after FN", pos);
        }
        auto funcName = readIdentifier(b, pos);
        
        // Must be followed by parentheses for user function call
        return parseFunction(funcName, b, pos, env);
    }

    // Identifier -> variable, function call, or array access
    if (isAsciiAlpha(t)) {
        size_t savedPos = pos;
        auto id = readIdentifier(b, pos);
        
        // Check if this is followed by an opening parenthesis (ASCII or tokenized)
        skipSpaces(b, pos);
        bool hasOpenParen = false;
        if (!atEnd(b, pos)) {
            uint8_t np = b[pos];
            if (np == '(' || np == '[') {
                hasOpenParen = true;
            } else if (np >= 0x80 && tokenizer) {
                std::string tn = tokenizer->getTokenName(np);
                if (tn == "(" || tn == "[") hasOpenParen = true;
            }
        }
        if (hasOpenParen) {
            // Decide between function call and array access
            if (isKnownFunction(id)) {
                // Known function - always treat as function call
                return parseFunction(id, b, pos, env);
            } else {
                // Not a known function - could be array access
                // Try array access first, fall back to function if array doesn't exist
                pos = savedPos;
                id = readIdentifier(b, pos);
                skipSpaces(b, pos);
                
                try {
                    return parseArrayAccess(id, b, pos, env);
                } catch (const BasicError& e) {
                    // Array access failed, try as function call
                    pos = savedPos;
                    id = readIdentifier(b, pos);
                    skipSpaces(b, pos);
                    return parseFunction(id, b, pos, env);
                }
            }
        }
        
        // Not followed by parentheses - treat as simple variable
        pos = savedPos;
        id = readIdentifier(b, pos);
        
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
            // Check if it's a tokenized operator (single byte) or ASCII sequence (2 bytes)
            if (pos < b.size() && b[pos] >= 0x80 && tokenizer && tokenizer->getTokenName(b[pos]) == op.op) {
                pos += 1; // Tokenized operator is single byte
            } else {
                pos += 2; // ASCII sequence
            }
        } else if (op.op == "AND" || op.op == "OR" || op.op == "XOR" || op.op == "EQV" || op.op == "IMP" || op.op == "MOD") {
            // Check if it's a tokenized operator (single byte) or ASCII word
            if (pos < b.size() && b[pos] >= 0x80 && tokenizer && tokenizer->getTokenName(b[pos]) == op.op) {
                pos += 1; // Tokenized operator is single byte
            } else {
                pos += op.op.size(); // ASCII word
            }
        } else {
            pos += 1; // Single character operator (ASCII or tokenized)
        }
        skipSpaces(b, pos);

        int nextMin = op.rightAssoc ? op.rbp : (op.lbp + 1);
        auto rhs = parseExpression(b, pos, env, nextMin);

        // Apply operator
        if (isComparison(op.op)) {
            bool res = false;
            // Handle string comparisons
            if (std::holds_alternative<Str>(lhs) && std::holds_alternative<Str>(rhs)) {
                const auto& a = std::get<Str>(lhs).v;
                const auto& c = std::get<Str>(rhs).v;
                int cmp = a.compare(c);
                if (op.op == "=") res = (cmp == 0);
                else if (op.op == "<>") res = (cmp != 0);
                else if (op.op == "<") res = (cmp < 0);
                else if (op.op == ">") res = (cmp > 0);
                else if (op.op == "<=") res = (cmp <= 0);
                else if (op.op == ">=") res = (cmp >= 0);
            }
            // Handle numeric comparisons
            else if (isNumeric(lhs) && isNumeric(rhs)) {
                double a = toDouble(lhs);
                double c = toDouble(rhs);
                if (op.op == "=") res = (a == c);
                else if (op.op == "<>") res = (a != c);
                else if (op.op == "<") res = (a < c);
                else if (op.op == ">") res = (a > c);
                else if (op.op == "<=") res = (a <= c);
                else if (op.op == ">=") res = (a >= c);
            }
            // Type mismatch: mixing strings and numbers
            else {
                throw BasicError(13, "Type mismatch", pos);
            }
            lhs = Int16{static_cast<int16_t>(res ? -1 : 0)};
            skipSpaces(b, pos);
            continue;
        }

        if (op.op == "+" || op.op == "-" || op.op == "*" || op.op == "/" || op.op == "^" || op.op == "\\" || op.op == "MOD") {
            // Support string concatenation with '+' as in GW-BASIC
            if (op.op == "+" && std::holds_alternative<Str>(lhs) && std::holds_alternative<Str>(rhs)) {
                const auto& a = std::get<Str>(lhs).v;
                const auto& c = std::get<Str>(rhs).v;
                lhs = Str{a + c};
                skipSpaces(b, pos);
                continue;
            }
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

bool ExpressionEvaluator::isKnownFunction(const std::string& name) const {
    // List of known built-in functions that should always be treated as function calls
    // when followed by parentheses, never as array access
    static const std::unordered_set<std::string> functions = {
        "SIN", "COS", "TAN", "ATN", "EXP", "LOG", "SQR", "ABS", "SGN", "INT", "FIX", "RND",
        "LEN", "ASC", "CHR$", "STR$", "VAL", "LEFT$", "RIGHT$", "MID$", "INSTR", "STRING$",
        "SPACE$", "TAB", "SPC", "POS", "CSRLIN", "EOF", "LOC", "LOF", "FRE", "PEEK", "INP",
        "POINT", "SCREEN", "VARPTR", "VARPTR$", "INPUT$", "INKEY$", "TIME$", "DATE$",
        "TIMER", "CINT", "CSNG", "CDBL", "HEX$", "OCT$", "LSET", "RSET", "LTRIM$", "RTRIM$"
    };
    
    std::string upper = name;
    std::transform(upper.begin(), upper.end(), upper.begin(), 
                   [](unsigned char c) { return std::toupper(c); });
    
    return functions.find(upper) != functions.end();
}

Value ExpressionEvaluator::parseArrayAccess(const std::string& arrayName, const std::vector<uint8_t>& b, size_t& pos, Env& env) {
    // Parse array subscripts: A(1), B(2,3), etc.
    std::vector<Value> indices = parseArgumentList(b, pos, env);
    
    if (indices.empty()) {
        throw BasicError(2, "Syntax error: array subscripts expected", pos);
    }
    
    // Try external array resolver first
    if (env.getArrayElem) {
        Value result{};
        if (env.getArrayElem(arrayName, indices, result)) {
            return result;
        }
    }
    
    // Fallback: treat as undefined array
    throw BasicError(2, "Undefined array: " + arrayName, pos);
}

} // namespace expr
