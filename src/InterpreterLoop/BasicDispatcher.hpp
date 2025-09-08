#pragma once

#include <cstdint>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <cctype>
#include <fstream>
#include <sstream>
#include <iomanip>

#include "../Tokenizer/Tokenizer.hpp"
#include "../ProgramStore/ProgramStore.hpp"
#include "../ExpressionEvaluator/ExpressionEvaluator.hpp"
#include "../Runtime/VariableTable.hpp"
#include "../Runtime/StringHeap.hpp"
#include "../Runtime/Value.hpp"
#include "../Runtime/RuntimeStack.hpp"
#include "../NumericEngine/NumericEngine.hpp"

// A dispatcher for a subset of BASIC statements to exercise the loop.
// It supports:
// - PRINT (strings/numbers; separators: space/comma/semicolon)
// - LET and implied assignment (A=expr)
// - IF expr THEN lineno   (no ELSE yet)
// - GOTO lineno
// - FOR var = start TO end [STEP increment]
// - NEXT [var]
// - END/STOP
// Returns next-line override (0 for fallthrough). Throws on errors.
class BasicDispatcher {
public:
    using PrintCallback = std::function<void(const std::string&)>;
    
    BasicDispatcher(std::shared_ptr<Tokenizer> t, std::shared_ptr<ProgramStore> p = nullptr, PrintCallback printCb = nullptr)
        : tok(std::move(t)), prog(std::move(p)), ev(tok), vars(&deftbl), strHeap(heapBuf, sizeof(heapBuf)), printCallback(printCb) {
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
        return operator()(tokens, 0); // default to line 0 if not specified
    }
    
    uint16_t operator()(const std::vector<uint8_t>& tokens, uint16_t currentLineNumber) {
        size_t pos = 0;
        currentLine = currentLineNumber; // store for use by statement handlers
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
    std::shared_ptr<ProgramStore> prog;
    expr::ExpressionEvaluator ev;
    // Runtime variable storage and string heap
    gwbasic::DefaultTypeTable deftbl{};
    gwbasic::VariableTable vars;
    uint8_t heapBuf[8192]{};
    gwbasic::StringHeap strHeap;
    gwbasic::RuntimeStack runtimeStack;
    expr::Env env;
    PrintCallback printCallback;
    uint16_t currentLine = 0; // Current line number being executed

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

    // PRINT USING helper method for string formatting
    std::string formatStringWithPattern(const std::string& formatString, const std::string& value) {
        // Simple implementation - just return the value for now
        // TODO: Implement proper string field parsing (&, !, \...\)
        return value;
    }

    uint16_t handleStatement(const std::vector<uint8_t>& b, size_t& pos) {
    uint8_t t = b[pos++]; // consume token
    // Map through Tokenizer public name lookup
    std::string name = tok ? tok->getTokenName(t) : std::string{};
    
    if (name == "PRINT") return doPRINT(b, pos);
    if (name == "LET") return handleLet(b, pos, /*implied*/false);
    if (name == "IF") return doIF(b, pos);
    if (name == "GOTO") return doGOTO(b, pos);
    if (name == "FOR") return doFOR(b, pos);
    if (name == "NEXT") return doNEXT(b, pos);
    if (name == "GOSUB") return doGOSUB(b, pos);
    if (name == "RETURN") return doRETURN(b, pos);
    if (name == "ON") return doON(b, pos);
    if (name == "LOAD") return doLOAD(b, pos);
    if (name == "SAVE") return doSAVE(b, pos);
    if (name == "END" || name == "STOP") return 0xFFFF; // sentinel for caller to halt
        // Unhandled: fallthrough no-op
        return 0;
    }

    uint16_t doPRINT(const std::vector<uint8_t>& b, size_t& pos) {
        // Check for PRINT USING syntax
        skipSpaces(b, pos);
        if (!atEnd(b, pos)) {
            // Check if the next token is USING
            if (tok) {
                std::string nextTokenName = tok->getTokenName(b[pos]);
                if (nextTokenName == "USING") {
                    return doPRINTUSING(b, pos);
                }
            }
        }
        
        // Regular PRINT statement
        bool newLine = true; // default prints newline
        bool first = true;
        std::string output;
        
        while (!atEnd(b, pos)) {
            skipSpaces(b, pos);
            if (atEnd(b, pos)) break;
            if (b[pos] == 0xF6) { newLine = false; ++pos; continue; }  // semicolon token
            if (b[pos] == 0xF5) { output += "\t"; ++pos; first = false; continue; }  // comma token
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

    uint16_t doPRINTUSING(const std::vector<uint8_t>& b, size_t& pos) {
        // Consume the USING token
        ++pos;
        
        skipSpaces(b, pos);
        
        // Evaluate the format string expression
        auto formatRes = ev.evaluate(b, pos, env);
        pos = formatRes.nextPos;
        
        std::string formatString;
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Str>) {
                formatString = x.v;
            } else {
                // Convert numeric to string for format
                if constexpr (std::is_same_v<T, expr::Int16>) formatString = std::to_string(x.v);
                else if constexpr (std::is_same_v<T, expr::Single>) formatString = std::to_string(x.v);
                else if constexpr (std::is_same_v<T, expr::Double>) formatString = std::to_string(x.v);
            }
        }, formatRes.value);
        
        skipSpaces(b, pos);
        
        // Expect semicolon after format string (semicolon is tokenized as 0xF6)
        if (!atEnd(b, pos) && b[pos] == 0xF6) {
            ++pos;
        } else {
            throw expr::BasicError(2, "Expected ';' after USING format string", pos);
        }
        
        std::string output;
        bool first = true;
        bool newLine = true;
        
        // Process the value list
        while (!atEnd(b, pos)) {
            skipSpaces(b, pos);
            if (atEnd(b, pos)) break;
            
            // Handle separators
            if (b[pos] == 0xF6) {  // semicolon token
                newLine = false; 
                ++pos; 
                continue; 
            }
            if (b[pos] == 0xF5) {  // comma token
                // For PRINT USING, comma usually means next field, 
                // but we'll treat it as continuation
                ++pos; 
                first = false; 
                continue; 
            }
            
            // Evaluate the expression
            auto res = ev.evaluate(b, pos, env);
            pos = res.nextPos;
            
            // Format the value using the format string
            std::string formattedValue;
            std::visit([&](auto&& x) {
                using T = std::decay_t<decltype(x)>;
                if constexpr (std::is_same_v<T, expr::Str>) {
                    // String value - use string patterns in format
                    formattedValue = formatStringWithPattern(formatString, x.v);
                } else {
                    // Numeric value - use numeric patterns in format
                    NumericEngine numEngine;
                    NumericValue numVal;
                    if constexpr (std::is_same_v<T, expr::Int16>) numVal = Int16{x.v};
                    else if constexpr (std::is_same_v<T, expr::Single>) numVal = Single{x.v};
                    else if constexpr (std::is_same_v<T, expr::Double>) numVal = Double{x.v};
                    
                    formattedValue = numEngine.printUsing(formatString, numVal);
                }
            }, res.value);
            
            output += formattedValue;
            first = false;
            
            skipSpaces(b, pos);
            if (!atEnd(b, pos) && b[pos] == ':') { ++pos; break; }
            if (!atEnd(b, pos) && b[pos] == ';') { newLine = false; ++pos; }
            if (!atEnd(b, pos) && b[pos] == ',') { ++pos; }
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

    // LOAD "filename"
    uint16_t doLOAD(const std::vector<uint8_t>& b, size_t& pos) {
        if (pos >= b.size()) {
            std::cerr << "?Missing filename" << std::endl;
            return 0;
        }

        // Expect a string literal for filename
        std::string filename;
        if (b[pos] == '"') {
            pos++; // skip opening quote
            while (pos < b.size() && b[pos] != '"') {
                filename += static_cast<char>(b[pos++]);
            }
            if (pos < b.size() && b[pos] == '"') {
                pos++; // skip closing quote
            }
        } else {
            std::cerr << "?String expected" << std::endl;
            return 0;
        }

        // Open and read the file
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "?File not found" << std::endl;
            return 0;
        }

        // Clear current program
        if (prog) {
            prog->clear();
        }

        // Read file line by line
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            
            // Parse line number if present
            std::istringstream iss(line);
            std::string token;
            iss >> token;
            
            uint16_t lineNum = 0;
            if (std::isdigit(token[0])) {
                lineNum = static_cast<uint16_t>(std::stoul(token));
                // Get rest of line
                std::string restOfLine;
                std::getline(iss, restOfLine);
                if (!restOfLine.empty() && restOfLine[0] == ' ') {
                    restOfLine = restOfLine.substr(1); // remove leading space
                }
                
                // Tokenize and store the line
                if (tok && prog) {
                    auto tokenized = tok->tokenize(restOfLine);
                    // Convert tokens to byte vector and add null terminator
                    std::vector<uint8_t> bytes;
                    for (const auto& token : tokenized) {
                        bytes.insert(bytes.end(), token.bytes.begin(), token.bytes.end());
                    }
                    bytes.push_back(0x00); // null terminator
                    prog->insertLine(lineNum, bytes);
                }
            }
        }
        
        file.close();
        std::cout << "Ok" << std::endl;
        return 0;
    }

    // SAVE "filename"
    uint16_t doSAVE(const std::vector<uint8_t>& b, size_t& pos) {
        if (pos >= b.size()) {
            std::cerr << "?Missing filename" << std::endl;
            return 0;
        }

        // Expect a string literal for filename
        std::string filename;
        if (b[pos] == '"') {
            pos++; // skip opening quote
            while (pos < b.size() && b[pos] != '"') {
                filename += static_cast<char>(b[pos++]);
            }
            if (pos < b.size() && b[pos] == '"') {
                pos++; // skip closing quote
            }
        } else {
            std::cerr << "?String expected" << std::endl;
            return 0;
        }

        // Open file for writing
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "?Cannot create file" << std::endl;
            return 0;
        }

        // Write program lines
        if (prog) {
            for (auto it = prog->begin(); it != prog->end(); ++it) {
                uint16_t lineNum = it->lineNumber;
                const auto& tokens = it->tokens;
                
                file << lineNum << " ";
                
                // Detokenize the line
                if (tok) {
                    std::string detokenized = tok->detokenize(tokens);
                    file << detokenized << std::endl;
                }
            }
        }
        
        file.close();
        std::cout << "Ok" << std::endl;
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

    uint16_t doFOR(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        
        // Read the loop variable name
        auto varName = readIdentifier(b, pos);
        if (varName.empty()) throw expr::BasicError(2, "Expected variable name", pos);
        
        skipSpaces(b, pos);
        // Expect '=' (ASCII or tokenized)
        if (atEnd(b, pos) || !(
            b[pos] == '=' ||
            (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "=")
        )) {
            throw expr::BasicError(2, "Expected = in FOR statement", pos);
        }
        ++pos; // consume '='
        
        skipSpaces(b, pos);
        // Evaluate start value
        auto startRes = ev.evaluate(b, pos, env);
        pos = startRes.nextPos;
        
        skipSpaces(b, pos);
        // Expect TO keyword
        bool hasTo = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80) {
                std::string name = tok ? tok->getTokenName(t) : std::string{};
                if (name == "TO") { ++pos; hasTo = true; }
            }
        }
        if (!hasTo) {
            // Try ASCII "TO"
            auto save = pos;
            std::string to;
            for (int i = 0; i < 2 && !atEnd(b, pos); ++i) to.push_back(static_cast<char>(std::toupper(b[pos++])));
            if (to == "TO") hasTo = true; else pos = save;
        }
        if (!hasTo) throw expr::BasicError(2, "Expected TO in FOR statement", pos);
        
        skipSpaces(b, pos);
        // Evaluate end value  
        auto endRes = ev.evaluate(b, pos, env);
        pos = endRes.nextPos;
        
        skipSpaces(b, pos);
        // Check for optional STEP
        expr::Value stepVal = expr::Int16{1}; // default step
        bool hasStep = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80) {
                std::string name = tok ? tok->getTokenName(t) : std::string{};
                if (name == "STEP") { ++pos; hasStep = true; }
            }
        }
        if (!hasStep) {
            // Try ASCII "STEP"
            auto save = pos;
            std::string step;
            for (int i = 0; i < 4 && !atEnd(b, pos); ++i) step.push_back(static_cast<char>(std::toupper(b[pos++])));
            if (step == "STEP") hasStep = true; else pos = save;
        }
        if (hasStep) {
            skipSpaces(b, pos);
            auto stepRes = ev.evaluate(b, pos, env);
            pos = stepRes.nextPos;
            stepVal = stepRes.value;
        }
        
        // Store initial value in variable
        toRuntimeValue(varName, startRes.value);
        
        // Check if we should even enter the loop
        // Convert values to double for comparison
        double startD = expr::ExpressionEvaluator::toDouble(startRes.value);
        double endD = expr::ExpressionEvaluator::toDouble(endRes.value);
        double stepD = expr::ExpressionEvaluator::toDouble(stepVal);
        
        // Check loop condition (ANSI BASIC semantics)
        bool shouldEnter = (stepD >= 0) ? (startD <= endD) : (startD >= endD);
        
        if (!shouldEnter) {
            // Skip to matching NEXT - for now, just scan ahead and consume the NEXT
            // In a real implementation, we'd scan to the matching NEXT statement
            return 0; // Skip loop entirely
        }
        
        // Create FOR frame and push onto stack
        gwbasic::ForFrame frame{};
        frame.varKey = varName;
        frame.control = toRuntimeValue(varName, startRes.value); // store current value
        frame.limit = toRuntimeValue("", endRes.value); // temporary storage for limit
        frame.step = toRuntimeValue("", stepVal); // temporary storage for step
        
        // Find the next line after the current FOR line - this is where the loop body starts
        uint16_t nextLine = 0;
        if (prog) {
            auto nextIt = prog->getNextLine(currentLine);
            if (nextIt.isValid()) {
                nextLine = nextIt->lineNumber;
            }
        }
        frame.textPtr = static_cast<uint32_t>(nextLine); // line number to jump back to (start of loop body)
        
        runtimeStack.pushFor(frame);
        
        return 0; // Continue to next statement
    }
    
    uint16_t doNEXT(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        
        // Optional variable name
        std::string varName;
        if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            varName = readIdentifier(b, pos);
        }
        
        // Find matching FOR frame
        gwbasic::ForFrame* forFrame = runtimeStack.topFor();
        if (!forFrame) {
            throw expr::BasicError(1, "NEXT without FOR", pos);
        }
        
        // If variable specified, check it matches
        if (!varName.empty() && forFrame->varKey != varName) {
            throw expr::BasicError(1, "NEXT variable mismatch", pos);
        }
        
        // Get current variable value
        auto* slot = vars.tryGet(forFrame->varKey);
        if (!slot) {
            throw expr::BasicError(1, "FOR variable not found", pos);
        }
        
        // Increment by step
        double current = 0;
        double step = 0;
        double limit = 0;
        
        switch (slot->scalar.type) {
            case gwbasic::ScalarType::Int16:
                current = slot->scalar.i;
                break;
            case gwbasic::ScalarType::Single:
                current = slot->scalar.f;
                break;
            case gwbasic::ScalarType::Double:
                current = slot->scalar.d;
                break;
            default:
                throw expr::BasicError(13, "Type mismatch in FOR loop", pos);
        }
        
        switch (forFrame->step.type) {
            case gwbasic::ScalarType::Int16:
                step = forFrame->step.i;
                break;
            case gwbasic::ScalarType::Single:
                step = forFrame->step.f;
                break;
            case gwbasic::ScalarType::Double:
                step = forFrame->step.d;
                break;
            default:
                step = 1;
        }
        
        switch (forFrame->limit.type) {
            case gwbasic::ScalarType::Int16:
                limit = forFrame->limit.i;
                break;
            case gwbasic::ScalarType::Single:
                limit = forFrame->limit.f;
                break;
            case gwbasic::ScalarType::Double:
                limit = forFrame->limit.d;
                break;
            default:
                limit = 0;
        }
        
        // Update loop variable
        current += step;
        
        // Store updated value back to variable
        switch (slot->scalar.type) {
            case gwbasic::ScalarType::Int16:
                slot->scalar.i = static_cast<int16_t>(current);
                break;
            case gwbasic::ScalarType::Single:
                slot->scalar.f = static_cast<float>(current);
                break;
            case gwbasic::ScalarType::Double:
                slot->scalar.d = current;
                break;
            default:
                break;
        }
        
        // Check loop termination condition
        bool shouldContinue = (step >= 0) ? (current <= limit) : (current >= limit);
        
        if (shouldContinue) {
            // Continue loop - jump back to start of FOR body
            uint16_t jumpTarget = static_cast<uint16_t>(forFrame->textPtr);
            return jumpTarget; // Jump back to the first line of the loop body
        } else {
            // Loop finished - pop FOR frame and continue
            gwbasic::ForFrame dummy;
            runtimeStack.popFor(dummy);
            return 0; // Continue to statement after NEXT
        }
    }
    
    uint16_t doGOSUB(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        
        // Read target line number
        uint16_t targetLine = readLineNumber(b, pos);
        
        // Create GOSUB frame - save current line and next statement position
        gwbasic::GosubFrame frame{};
        frame.returnLine = currentLine;
        frame.returnTextPtr = 0; // We'll use this to indicate we should continue to next line
        
        // Push frame onto GOSUB stack
        runtimeStack.pushGosub(frame);
        
        // Jump to target line
        return targetLine;
    }
    
    uint16_t doRETURN(const std::vector<uint8_t>& /*b*/, size_t& /*pos*/) {
        // Pop GOSUB frame
        gwbasic::GosubFrame frame{};
        if (!runtimeStack.popGosub(frame)) {
            throw expr::BasicError(3, "RETURN without GOSUB", 0);
        }
        
        // Return to the line after the GOSUB
        if (prog && frame.returnLine != 0) {
            auto nextIt = prog->getNextLine(frame.returnLine);
            if (nextIt.isValid()) {
                return nextIt->lineNumber;
            }
        }
        
        // If we can't find the next line, just continue normally
        return 0;
    }
    
    uint16_t doON(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        
        // Evaluate the expression to get the index
        auto res = ev.evaluate(b, pos, env);
        pos = res.nextPos;
        
        // Convert to integer index (1-based)
        int16_t index = expr::ExpressionEvaluator::toInt16(res.value);
        
        skipSpaces(b, pos);
        
        // Check for GOTO or GOSUB keyword
        bool isGosub = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            
            if (t >= 0x80) {
                std::string name = tok ? tok->getTokenName(t) : std::string{};
                if (name == "GOTO") {
                    ++pos;
                } else if (name == "GOSUB") {
                    ++pos;
                    isGosub = true;
                } else {
                    throw expr::BasicError(2, "Expected GOTO or GOSUB in ON statement", pos);
                }
            } else {
                // Try ASCII
                auto save = pos;
                std::string keyword;
                while (!atEnd(b, pos) && std::isalpha(b[pos])) {
                    keyword.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
                if (keyword == "GOTO") {
                    // keep pos at current position
                } else if (keyword == "GOSUB") {
                    isGosub = true;
                } else {
                    pos = save;
                    throw expr::BasicError(2, "Expected GOTO or GOSUB in ON statement", pos);
                }
            }
        } else {
            throw expr::BasicError(2, "Expected GOTO or GOSUB in ON statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse line number list - look for sequence of integer tokens
        std::vector<uint16_t> lineNumbers;
        
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) break;
            
            // Check if we have an integer token (0x11) or ASCII digits
            if (pos + 2 < b.size() && b[pos] == 0x11) {
                // Integer token - read it
                uint16_t lineNum = static_cast<uint16_t>(b[pos+1]) | (static_cast<uint16_t>(b[pos+2]) << 8);
                lineNumbers.push_back(lineNum);
                pos += 3;
            } else if (pos < b.size() && b[pos] >= '0' && b[pos] <= '9') {
                // ASCII digits
                uint32_t v = 0;
                while (pos < b.size() && b[pos] >= '0' && b[pos] <= '9') { 
                    v = v*10 + (b[pos]-'0'); 
                    ++pos; 
                }
                lineNumbers.push_back(static_cast<uint16_t>(v));
            } else {
                // Check for comma separator
                if (b[pos] == ',' || b[pos] == 0xF5) { // 0xF5 is tokenized comma
                    ++pos; // consume comma and continue
                    continue;
                } else {
                    // End of line number list
                    break;
                }
            }
            
            skipSpaces(b, pos);
            
            // Optional comma - if present, consume it
            if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5)) {
                ++pos; // consume comma
                continue;
            }
            // If no comma, we might be at the end or there might be more integer tokens
            // Continue the loop to check for more integer tokens
        }
        
        // If index is out of range (0 or negative, or beyond list), do nothing
        if (index <= 0 || index > static_cast<int16_t>(lineNumbers.size())) {
            return 0; // No jump
        }
        
        // Get the target line number (1-based indexing)
        uint16_t targetLine = lineNumbers[static_cast<size_t>(index - 1)];
        
        if (isGosub) {
            // Push GOSUB frame before jumping
            gwbasic::GosubFrame frame{};
            frame.returnLine = currentLine;
            frame.returnTextPtr = 0;
            runtimeStack.pushGosub(frame);
        }
        
        return targetLine;
    }
};
