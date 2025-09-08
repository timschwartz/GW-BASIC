#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cctype>

#include "../Tokenizer/Tokenizer.hpp"
#include "../ExpressionEvaluator/ExpressionEvaluator.hpp"
#include "../Runtime/VariableTable.hpp"
#include "../Runtime/StringHeap.hpp"
#include "../Runtime/Value.hpp"

// A tiny, minimal dispatcher for a subset of BASIC statements to exercise the loop.
// It supports:
// - PRINT (strings/numbers; separators: space/comma/semicolon)
// - LET and implied assignment (A=expr)
// - IF expr THEN lineno   (no ELSE yet)
// - GOTO lineno
// - END/STOP
// Returns next-line override (0 for fallthrough). Throws on errors.
class BasicDispatcher {
public:
    using PrintCallback = std::function<void(const std::string&)>;
    
    BasicDispatcher(std::shared_ptr<Tokenizer> t, PrintCallback printCb = nullptr)
        : tok(std::move(t)), ev(tok), vars(&deftbl), strHeap(heapBuf, sizeof(heapBuf)), printCallback(printCb) {
        // Wire evaluator env to read variables from VariableTable
        env.optionBase = 0;
        env.vars.clear();
        env.getVar = [this](const std::string& name, expr::Value& out) -> bool {
            if (auto* slot = vars.tryGet(name)) {
                out = toExprValue(slot->scalar);
                return true;
            }
            return false;
        };
    }

    uint16_t operator()(const std::vector<uint8_t>& tokens) {
        size_t pos = 0;
        expr::Env envRef = env; // local view for eval; share vars by reference via pointer below if needed
        // peek first significant byte
        skipSpaces(tokens, pos);
        // Some sources may include a leading tokenized line-number (0x0D LL HH); skip it defensively
        if (pos + 2 < tokens.size() && tokens[pos] == 0x0D) {
            pos += 3;
            skipSpaces(tokens, pos);
        }
        if (atEnd(tokens, pos)) return 0;

        // Execute statements separated by ':' until EOL or a jump/termination happens
        while (!atEnd(tokens, pos)) {
            skipSpaces(tokens, pos);
            if (atEnd(tokens, pos)) break;
            if (tokens[pos] == ':') { ++pos; continue; }
            uint8_t b = tokens[pos];
            uint16_t r = 0;
            if (b >= 0x80) {
                r = handleStatement(tokens, pos);
            } else {
                r = handleLet(tokens, pos, /*implied*/true);
            }
            if (r != 0) return r; // jump or termination sentinel
            skipSpaces(tokens, pos);
            if (!atEnd(tokens, pos) && tokens[pos] == ':') { ++pos; continue; }
            // If not colon, continue loop which will finish if atEnd
        }
        return 0;
    }

    // Expose environment for inspection in tests
    expr::Env& environment() { return env; }

private:
    std::shared_ptr<Tokenizer> tok;
    expr::ExpressionEvaluator ev;
    // Runtime variable storage and string heap
    gwbasic::DefaultTypeTable deftbl{};
    gwbasic::VariableTable vars;
    uint8_t heapBuf[8192]{};
    gwbasic::StringHeap strHeap;
    expr::Env env;
    PrintCallback printCallback;

    // Helpers to convert between runtime Value and evaluator Value
    static expr::Value toExprValue(const gwbasic::Value& v) {
        using gwbasic::ScalarType;
        switch (v.type) {
            case ScalarType::Int16:  return expr::Int16{v.i};
            case ScalarType::Single: return expr::Single{v.f};
            case ScalarType::Double: return expr::Double{v.d};
            case ScalarType::String:
                return expr::Str{std::string(reinterpret_cast<const char*>(v.s.ptr), reinterpret_cast<const char*>(v.s.ptr) + v.s.len)};
        }
        return expr::Int16{0};
    }

    gwbasic::Value toRuntimeValue(const std::string& varName, const expr::Value& v) {
        // Determine target type from variable table entry
        gwbasic::VarSlot& slot = vars.getOrCreate(varName);
        using gwbasic::ScalarType;
        gwbasic::Value out{};
        switch (slot.scalar.type) {
            case ScalarType::Int16:
                out = gwbasic::Value::makeInt(expr::ExpressionEvaluator::toInt16(v));
                break;
            case ScalarType::Single:
                out = gwbasic::Value::makeSingle(static_cast<float>(expr::ExpressionEvaluator::toDouble(v)));
                break;
            case ScalarType::Double:
                out = gwbasic::Value::makeDouble(expr::ExpressionEvaluator::toDouble(v));
                break;
            case ScalarType::String: {
                // Allocate string in runtime heap
                std::string s = std::visit([](auto&& x)->std::string{
                    using T = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<T, expr::Str>) return x.v;
                    else throw expr::BasicError(13, "Type mismatch", 0);
                }, v);
                gwbasic::StrDesc sd{};
                if (!strHeap.allocCopy(reinterpret_cast<const uint8_t*>(s.data()), static_cast<uint16_t>(s.size()), sd)) {
                    throw expr::BasicError(7, "Out of string space", 0);
                }
                out = gwbasic::Value::makeString(sd);
                break;
            }
        }
        // Store back to table
        slot.scalar = out;
        return out;
    }

    static bool atEnd(const std::vector<uint8_t>& b, size_t pos) { return pos >= b.size() || b[pos] == 0x00; }
    static bool isSpace(uint8_t c) { return c==' '||c=='\t'||c=='\r'||c=='\n'; }
    static void skipSpaces(const std::vector<uint8_t>& b, size_t& pos) { while (pos < b.size() && isSpace(b[pos])) ++pos; }

    uint16_t handleStatement(const std::vector<uint8_t>& b, size_t& pos) {
    uint8_t t = b[pos++]; // consume token
    // Map through Tokenizer public name lookup
    std::string name = tok ? tok->getTokenName(t) : std::string{};
    if (name == "PRINT") return doPRINT(b, pos);
    if (name == "LET") return handleLet(b, pos, /*implied*/false);
    if (name == "IF") return doIF(b, pos);
    if (name == "GOTO") return doGOTO(b, pos);
    if (name == "END" || name == "STOP") return 0xFFFF; // sentinel for caller to halt
        // Unhandled: fallthrough no-op
        return 0;
    }

    uint16_t doPRINT(const std::vector<uint8_t>& b, size_t& pos) {
        bool newLine = true; // default prints newline
        bool first = true;
        std::string output;
        
        while (!atEnd(b, pos)) {
            skipSpaces(b, pos);
            if (atEnd(b, pos)) break;
            if (b[pos] == ';') { newLine = false; ++pos; continue; }
            if (b[pos] == ',') { output += "\t"; ++pos; first = false; continue; }
            // Expression
            auto res = ev.evaluate(b, pos, env);
            pos = res.nextPos;
            std::visit([&](auto&& x){ 
                using T = std::decay_t<decltype(x)>; 
                if constexpr (std::is_same_v<T, expr::Str>) output += x.v; 
                else if constexpr (std::is_same_v<T, expr::Int16>) output += std::to_string(x.v); 
                else if constexpr (std::is_same_v<T, expr::Single>) output += std::to_string(x.v); 
                else if constexpr (std::is_same_v<T, expr::Double>) output += std::to_string(x.v); 
            }, res.value);
            first = false;
            skipSpaces(b, pos);
            if (!atEnd(b, pos) && b[pos] == ':') { ++pos; break; }
            if (!atEnd(b, pos) && b[pos] == ';') { newLine = false; ++pos; }
            if (!atEnd(b, pos) && b[pos] == ',') { output += "\t"; ++pos; }
        }
        if (newLine) output += "\n";
        
        // Send output to callback if available, otherwise fall back to cout
        if (printCallback) {
            printCallback(output);
        } else {
            std::cout << output;
        }
        
        return 0;
    }

    static std::string readIdentifier(const std::vector<uint8_t>& b, size_t& pos) {
        std::string id;
        if (pos < b.size() && ((b[pos] >= 'A' && b[pos] <= 'Z') || (b[pos] >= 'a' && b[pos] <= 'z'))) {
            id.push_back(static_cast<char>(b[pos++]));
            while (pos < b.size()) {
                uint8_t c = b[pos];
                if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c=='$' || c=='%' || c=='!' || c=='#') {
                    id.push_back(static_cast<char>(c));
                    ++pos;
                } else break;
            }
        }
        return id;
    }

    uint16_t handleLet(const std::vector<uint8_t>& b, size_t& pos, bool implied) {
        skipSpaces(b, pos);
        auto name = readIdentifier(b, pos);
        if (name.empty()) throw expr::BasicError(2, "Syntax error", pos);
        skipSpaces(b, pos);
        if (atEnd(b, pos) || !(
            b[pos] == '=' ||
            (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "=")
        )) {
            if (implied) throw expr::BasicError(2, "Syntax error: expected =", pos);
            // plain LET with no assignment? treat as no-op
            return 0;
        }
        ++pos; // consume '=' (ASCII or tokenized)
        skipSpaces(b, pos);
    auto res = ev.evaluate(b, pos, env);
        // Store into runtime variable table
        toRuntimeValue(name, res.value);
        pos = res.nextPos;
        return 0;
    }

    static uint16_t readLineNumber(const std::vector<uint8_t>& b, size_t& pos) {
        // Accept either tokenized line marker 0x0D, numeric constant 0x11 (int), or ASCII digits
        if (pos < b.size() && b[pos] == 0x0D) {
            if (pos + 2 >= b.size()) throw expr::BasicError(2, "Bad line number", pos);
            uint16_t v = static_cast<uint16_t>(b[pos+1]) | (static_cast<uint16_t>(b[pos+2]) << 8);
            pos += 3; return v;
        }
        if (pos + 2 < b.size() && b[pos] == 0x11) {
            uint16_t v = static_cast<uint16_t>(b[pos+1]) | (static_cast<uint16_t>(b[pos+2]) << 8);
            pos += 3; return v;
        }
        // ASCII digits
        uint32_t v = 0; bool any = false;
        while (pos < b.size() && b[pos] >= '0' && b[pos] <= '9') { any = true; v = v*10 + (b[pos]-'0'); ++pos; }
        if (!any) throw expr::BasicError(2, "Bad line number", pos);
        return static_cast<uint16_t>(v);
    }

    uint16_t doGOTO(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        return readLineNumber(b, pos);
    }

    uint16_t doIF(const std::vector<uint8_t>& b, size_t& pos) {
    skipSpaces(b, pos);
    auto res = ev.evaluate(b, pos, env);
    pos = res.nextPos;
        // Expect THEN
        skipSpaces(b, pos);
        // Accept tokenized THEN or ASCII THEN
        bool hasThen = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80) {
                std::string name = tok ? tok->getTokenName(t) : std::string{};
                if (name == "THEN") { ++pos; hasThen = true; }
            }
        }
        if (!hasThen) {
            auto save = pos;
            std::string then;
            for (int i = 0; i < 4 && !atEnd(b, pos); ++i) then.push_back(static_cast<char>(std::toupper(b[pos++])));
            if (then == "THEN") hasThen = true; else pos = save;
        }
    if (!hasThen) { throw expr::BasicError(2, "Missing THEN", pos); }
        skipSpaces(b, pos);
        auto condTrue = expr::ExpressionEvaluator::toBoolInt(res.value) != 0;

        // Helper lambdas
        auto isElseAt = [&](size_t p)->bool{
            if (p >= b.size()) return false;
            if (b[p] >= 0x80) {
                std::string n = tok ? tok->getTokenName(b[p]) : std::string{};
                return n == "ELSE";
            }
            // ASCII ELSE
            if (p + 3 < b.size()) {
                return (std::toupper(b[p])=='E' && std::toupper(b[p+1])=='L' && std::toupper(b[p+2])=='S' && std::toupper(b[p+3])=='E');
            }
            return false;
        };

        auto skipAsciiWord = [&](size_t& p, const char* w){ size_t i=0; while (w[i] && p < b.size() && std::toupper(b[p])==std::toupper(w[i])) { ++p; ++i; } };

        auto findBranchEnd = [&](size_t p)->size_t{
            while (p < b.size() && b[p] != 0x00) {
                if (b[p] == '"') { // skip string
                    ++p; while (p < b.size() && b[p] != 0x00 && b[p] != '"') ++p; if (p < b.size() && b[p]=='"') ++p; continue;
                }
                if (b[p] == ':') return p; // end of statement
                if (isElseAt(p)) return p; // ELSE starts next branch
                ++p;
            }
            return p;
        };

        // If next token is a line number, treat as jump target
    auto peekIsLineNumber = [&](size_t p)->bool{ return p < b.size() && (b[p] == 0x0D || b[p] == 0x11 || (b[p] >= '0' && b[p] <= '9')); };

        if (condTrue) {
            if (peekIsLineNumber(pos)) {
                return readLineNumber(b, pos);
            }
            // Inline THEN statement
            size_t end = findBranchEnd(pos);
            if (end > pos) {
                std::vector<uint8_t> sub(b.begin() + static_cast<ptrdiff_t>(pos), b.begin() + static_cast<ptrdiff_t>(end));
                sub.push_back(0x00);
                // Execute inline
                auto r = (*this)(sub);
                if (r != 0) return r;
            }
            // Skip THEN branch and optional ELSE part entirely
            pos = end;
            // If ELSE present, skip it and its branch
            if (isElseAt(pos)) {
                if (b[pos] >= 0x80) { ++pos; } else { skipAsciiWord(pos, "ELSE"); }
                // Skip else branch tokens
                size_t elseEnd = findBranchEnd(pos);
                pos = elseEnd;
            }
            // We've handled the inline THEN; do not re-run it in outer loop. If a colon follows, outer loop will continue to next stmt.
            return 0;
        } else {
            // Skip THEN branch
            if (peekIsLineNumber(pos)) {
                (void)readLineNumber(b, pos); // consume
            } else {
                size_t end = findBranchEnd(pos);
                pos = end;
            }
            skipSpaces(b, pos);
            // Check for ELSE
            if (isElseAt(pos)) {
                if (b[pos] >= 0x80) { ++pos; } else { skipAsciiWord(pos, "ELSE"); }
                skipSpaces(b, pos);
                if (peekIsLineNumber(pos)) { return readLineNumber(b, pos); }
                // Inline ELSE statement
                size_t end = findBranchEnd(pos);
                if (end > pos) {
                    std::vector<uint8_t> sub(b.begin() + static_cast<ptrdiff_t>(pos), b.begin() + static_cast<ptrdiff_t>(end));
                    sub.push_back(0x00);
                    auto r = (*this)(sub);
                    if (r != 0) return r;
                }
                // Advance position past the ELSE inline branch so the outer loop does not execute it again
                pos = end;
            }
            // After ELSE inline execution or skip, return to outer to proceed after colon if present.
            return 0;
        }
    }
};
