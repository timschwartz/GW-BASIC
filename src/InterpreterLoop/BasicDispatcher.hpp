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
#include "../Runtime/ArrayManager.hpp"
#include "../Runtime/EventTraps.hpp"
#include "../Runtime/DataManager.hpp"
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
    using InputCallback = std::function<std::string(const std::string&)>; // prompt -> input
    
    BasicDispatcher(std::shared_ptr<Tokenizer> t, std::shared_ptr<ProgramStore> p = nullptr, PrintCallback printCb = nullptr, InputCallback inputCb = nullptr)
        : tok(std::move(t)), prog(std::move(p)), ev(tok), vars(&deftbl), strHeap(heapBuf, sizeof(heapBuf)), arrayManager(&strHeap), eventTraps(), dataManager(prog, tok), printCallback(printCb), inputCallback(inputCb) {
        // Wire up cross-references
        vars.setStringHeap(&strHeap);
        vars.setArrayManager(&arrayManager);
        
        // Wire evaluator env to read variables from VariableTable
        env.optionBase = 0;
        env.vars.clear();
        env.getVar = [this](const std::string& name, expr::Value& out) -> bool {
            if (auto* slot = vars.tryGet(name)) {
                if (!slot->isArray) {
                    out = toExprValue(slot->scalar);
                    return true;
                }
            }
            return false;
        };
        
        // Wire array element access
        env.getArrayElem = [this](const std::string& name, const std::vector<expr::Value>& indices, expr::Value& out) -> bool {
            // Convert expression indices to runtime indices
            std::vector<int32_t> runtimeIndices;
            for (const auto& idx : indices) {
                runtimeIndices.push_back(expr::ExpressionEvaluator::toInt16(idx));
            }
            
            gwbasic::Value value;
            if (vars.getArrayElement(name, runtimeIndices, value)) {
                out = toExprValue(value);
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

    // Access to event trap system
    gwbasic::EventTrapSystem* getEventTrapSystem() { return &eventTraps; }
    
    // Access to data manager for program loading
    void resetDataManager() { dataManager.restore(); }
    
    // Set test mode to avoid waiting for input
    void setTestMode(bool enabled) { testMode = enabled; }

private:
    std::shared_ptr<Tokenizer> tok;
    std::shared_ptr<ProgramStore> prog;
    expr::ExpressionEvaluator ev;
    // Runtime variable storage and string heap
    gwbasic::DefaultTypeTable deftbl{};
    gwbasic::VariableTable vars;
    uint8_t heapBuf[8192]{};
    gwbasic::StringHeap strHeap;
    gwbasic::ArrayManager arrayManager;
    gwbasic::RuntimeStack runtimeStack;
    gwbasic::EventTrapSystem eventTraps;
    gwbasic::DataManager dataManager;
    expr::Env env;
    PrintCallback printCallback;
    InputCallback inputCallback;
    uint16_t currentLine = 0; // Current line number being executed
    bool testMode = false; // Set to true to avoid waiting for input in tests

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
                std::string s = std::visit([this](auto&& x)->std::string{
                    using T = std::decay_t<decltype(x)>;
                    if constexpr (std::is_same_v<T, expr::Str>) return x.v;
                    else {
                        this->throwBasicError(13, "Type mismatch", 0);
                        return std::string{}; // Never reached, but needed for compile
                    }
                }, v);
                gwbasic::StrDesc sd{};
                if (!strHeap.allocCopy(reinterpret_cast<const uint8_t*>(s.data()), static_cast<uint16_t>(s.size()), sd)) {
                    throwBasicError(7, "Out of string space", 0);
                }
                out = gwbasic::Value::makeString(sd);
                break;
            }
        }
        // Store back to table
        slot.scalar = out;
        return out;
    }

    gwbasic::Value toRuntimeValueForArray(const std::string& arrayName, const expr::Value& v) {
        // For array assignment, determine type from the array's type rather than variable table
        // since array elements must match the array's type
        
        gwbasic::Value out{};
        std::visit([&](auto&& x)->void{
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Int16>) {
                out = gwbasic::Value::makeInt(x.v);
            } else if constexpr (std::is_same_v<T, expr::Single>) {
                out = gwbasic::Value::makeSingle(x.v);
            } else if constexpr (std::is_same_v<T, expr::Double>) {
                out = gwbasic::Value::makeDouble(x.v);
            } else if constexpr (std::is_same_v<T, expr::Str>) {
                // Allocate string in runtime heap
                gwbasic::StrDesc sd{};
                if (!strHeap.allocCopy(reinterpret_cast<const uint8_t*>(x.v.data()), static_cast<uint16_t>(x.v.size()), sd)) {
                    throwBasicError(7, "Out of string space", 0);
                }
                out = gwbasic::Value::makeString(sd);
            }
        }, v);
        return out;
    }

    static bool atEnd(const std::vector<uint8_t>& b, size_t pos) { 
        // Only check for end of vector, not embedded 0x00 bytes
        // 0x00 bytes can appear in tokenized integers and should not terminate parsing
        return pos >= b.size(); 
    }
    static bool isSpace(uint8_t c) { return c==' '||c=='\t'||c=='\r'||c=='\n'; }
    static void skipSpaces(const std::vector<uint8_t>& b, size_t& pos) { while (pos < b.size() && isSpace(b[pos])) ++pos; }

    // PRINT USING helper method for string formatting
    std::string formatStringWithPattern(const std::string& formatString, const std::string& value) {
        // Simple implementation - just return the value for now
        // TODO: Implement proper string field parsing (&, !, \...\)
        return value;
    }

    // Helper method to send error messages to both console and SDL output
    void reportError(const std::string& errorMessage) {
        std::string fullMessage;
        if (currentLine > 0) {
            fullMessage = "Error in line " + std::to_string(currentLine) + ": " + errorMessage;
        } else {
            fullMessage = "Error: " + errorMessage;
        }
        
        // Send to console stderr
        std::cerr << fullMessage << std::endl;
        
        // Send to SDL output via printCallback
        if (printCallback) {
            printCallback(fullMessage + "\n");
        }
    }

    // Helper method to create and throw BasicError with dual output
    void throwBasicError(int code, const std::string& message, size_t position) {
        reportError(message);
        throw expr::BasicError(code, message, position);
    }

    uint16_t handleStatement(const std::vector<uint8_t>& b, size_t& pos) {
    uint8_t t = b[pos++]; // consume token
    // Map through Tokenizer public name lookup
    std::string name = tok ? tok->getTokenName(t) : std::string{};
    
    if (name == "PRINT") return doPRINT(b, pos);
    if (name == "INPUT") return doINPUT(b, pos);
    if (name == "LET") return handleLet(b, pos, /*implied*/false);
    if (name == "DIM") return doDIM(b, pos);
    if (name == "READ") return doREAD(b, pos);
    if (name == "DATA") return doDATA(b, pos);
    if (name == "RESTORE") return doRESTORE(b, pos);
    if (name == "IF") return doIF(b, pos);
    if (name == "GOTO") return doGOTO(b, pos);
    if (name == "FOR") return doFOR(b, pos);
    if (name == "NEXT") return doNEXT(b, pos);
    if (name == "GOSUB") return doGOSUB(b, pos);
    if (name == "RETURN") return doRETURN(b, pos);
    if (name == "ON") return doON(b, pos);
    if (name == "LOAD") return doLOAD(b, pos);
    if (name == "SAVE") return doSAVE(b, pos);
    if (name == "ERROR") return doERROR(b, pos);
    if (name == "RESUME") return doRESUME(b, pos);
    if (name == "KEY") return doKEY(b, pos);
    if (name == "TIMER") return doTIMER(b, pos);
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
        
        // Send output to both callback and stdout
        if (printCallback) {
            printCallback(output);
        }
        std::cout << output; // Always output to stdout as well
        
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
            throwBasicError(2, "Expected ';' after USING format string", pos);
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
                    if constexpr (std::is_same_v<T, expr::Int16>) {
                        numVal = Int16{x.v};
                    } else if constexpr (std::is_same_v<T, expr::Single>) {
                        numVal = Single{x.v};
                    } else if constexpr (std::is_same_v<T, expr::Double>) {
                        numVal = Double{x.v};
                    }
                    
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
        
        // Send output to both callback and stdout
        if (printCallback) {
            printCallback(output);
        }
        std::cout << output; // Always output to stdout as well
        
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
        if (name.empty()) throwBasicError(2, "Syntax error", pos);
        skipSpaces(b, pos);
        
        // Check if this is an array assignment: VAR(indices) = expr
        if (!atEnd(b, pos) && (b[pos] == '(' || b[pos] == '[' || b[pos] == 0xf3)) {
            // Array assignment
            char openBracket = '\0';
            char closeBracket = '\0';
            bool isTokenized = false;
            
            if (b[pos] == '(' || b[pos] == '[') {
                openBracket = static_cast<char>(b[pos]);
                closeBracket = (openBracket == '(') ? ')' : ']';
                isTokenized = false;
                ++pos; // consume opening bracket
            } else if (b[pos] == 0xf3) {  // tokenized left parenthesis
                openBracket = '(';
                closeBracket = ')';
                isTokenized = true;
                ++pos; // consume opening bracket token
            }
            
            // Parse indices
            std::vector<int32_t> indices;
            
            // Define a lambda to check for closing bracket
            auto isClosingBracket = [&](uint8_t ch) -> bool {
                if (!isTokenized) {
                    // We started with ASCII, expect ASCII closing
                    return ch == closeBracket;
                } else {
                    // We started with tokenized parenthesis, expect tokenized closing
                    return ch == 0xf4; // tokenized right parenthesis
                }
            };
            
            while (!atEnd(b, pos) && !isClosingBracket(b[pos])) {
                skipSpaces(b, pos);
                if (atEnd(b, pos)) {
                    throwBasicError(2, "Missing closing bracket in array assignment", pos);
                }
                
                // Evaluate index expression
                auto res = ev.evaluate(b, pos, env);
                pos = res.nextPos;
                
                indices.push_back(expr::ExpressionEvaluator::toInt16(res.value));
                
                skipSpaces(b, pos);
                
                // Check for comma (more indices)
                if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5)) { // 0xF5 is tokenized comma
                    ++pos; // consume comma
                    continue;
                }
                
                break;
            }
            
            // Expect closing bracket
            bool found_closing_bracket = false;
            if (!isTokenized) {
                // We started with ASCII, expect ASCII closing
                if (!atEnd(b, pos) && b[pos] == closeBracket) {
                    found_closing_bracket = true;
                    ++pos;
                }
            } else {
                // We started with tokenized parenthesis, expect tokenized closing
                if (!atEnd(b, pos) && b[pos] == 0xf4) { // tokenized right parenthesis
                    found_closing_bracket = true;
                    ++pos;
                }
            }
            
            if (!found_closing_bracket) {
                throwBasicError(2, "Missing closing bracket in array assignment", pos);
            }
            
            skipSpaces(b, pos);
            
            // Expect equals sign
            if (atEnd(b, pos) || !(
                b[pos] == '=' ||
                (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "=")
            )) {
                throwBasicError(2, "Expected = in array assignment", pos);
            }
            ++pos; // consume '='
            
            skipSpaces(b, pos);
            
            // Evaluate value expression
            auto res = ev.evaluate(b, pos, env);
            pos = res.nextPos;
            
            // Convert expression value to runtime value and store in array
            gwbasic::Value runtimeValue = toRuntimeValueForArray(name, res.value);
            if (!vars.setArrayElement(name, indices, runtimeValue)) {
                throwBasicError(9, "Subscript out of range or type mismatch", pos);
            }
            
            return 0;
        }
        
        // Regular scalar assignment
        if (atEnd(b, pos) || !(
            b[pos] == '=' ||
            (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "=")
        )) {
            if (implied) throwBasicError(2, "Syntax error: expected =", pos);
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
        
        // Check for event trap syntax first: ON ERROR, ON KEY, ON TIMER, etc.
        if (!atEnd(b, pos)) {
            auto savePos = pos;
            
            // Try to parse event type keyword
            std::string eventKeyword;
            uint8_t t = b[pos];
            
            if (t >= 0x80) {
                eventKeyword = tok ? tok->getTokenName(t) : std::string{};
            } else {
                // Try ASCII
                while (!atEnd(b, pos) && std::isalpha(b[pos])) {
                    eventKeyword.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
            }
            
            // Handle event trap statements
            if (eventKeyword == "ERROR") {
                // ON ERROR GOTO line
                if (t >= 0x80) ++pos; // consume token
                skipSpaces(b, pos);
                
                // Expect GOTO
                bool hasGoto = false;
                if (!atEnd(b, pos)) {
                    uint8_t t2 = b[pos];
                    if (t2 >= 0x80) {
                        std::string name = tok ? tok->getTokenName(t2) : std::string{};
                        if (name == "GOTO") { ++pos; hasGoto = true; }
                    } else {
                        // Try ASCII "GOTO"
                        auto save = pos;
                        std::string gotoWord;
                        for (int i = 0; i < 4 && !atEnd(b, pos); ++i) {
                            gotoWord.push_back(static_cast<char>(std::toupper(b[pos++])));
                        }
                        if (gotoWord == "GOTO") hasGoto = true; else pos = save;
                    }
                }
                
                if (!hasGoto) {
                    throw expr::BasicError(2, "Expected GOTO after ON ERROR", pos);
                }
                
                skipSpaces(b, pos);
                uint16_t handlerLine = readLineNumber(b, pos);
                
                // Set error trap
                eventTraps.setErrorTrap(handlerLine);
                runtimeStack.setErrorHandler(handlerLine);
                
                return 0;
                
            } else if (eventKeyword == "KEY") {
                // ON KEY(n) GOTO line
                if (t >= 0x80) ++pos; // consume token
                skipSpaces(b, pos);
                
                // Parse KEY(n) or just KEY
                uint8_t keyIndex = 0;
                if (!atEnd(b, pos) && (b[pos] == '(' || b[pos] == 0xf3)) {
                    // KEY(n) syntax
                    if (b[pos] == '(') ++pos;
                    else if (b[pos] == 0xf3) ++pos; // tokenized (
                    
                    skipSpaces(b, pos);
                    auto res = ev.evaluate(b, pos, env);
                    pos = res.nextPos;
                    keyIndex = static_cast<uint8_t>(expr::ExpressionEvaluator::toInt16(res.value));
                    
                    skipSpaces(b, pos);
                    if (!atEnd(b, pos) && (b[pos] == ')' || b[pos] == 0xf4)) {
                        ++pos; // consume closing )
                    }
                }
                
                skipSpaces(b, pos);
                
                // Expect GOTO
                bool hasGoto = false;
                if (!atEnd(b, pos)) {
                    uint8_t t2 = b[pos];
                    if (t2 >= 0x80) {
                        std::string name = tok ? tok->getTokenName(t2) : std::string{};
                        if (name == "GOTO") { ++pos; hasGoto = true; }
                    } else {
                        // Try ASCII "GOTO"
                        auto save = pos;
                        std::string gotoWord;
                        for (int i = 0; i < 4 && !atEnd(b, pos); ++i) {
                            gotoWord.push_back(static_cast<char>(std::toupper(b[pos++])));
                        }
                        if (gotoWord == "GOTO") hasGoto = true; else pos = save;
                    }
                }
                
                if (!hasGoto) {
                    throw expr::BasicError(2, "Expected GOTO after ON KEY", pos);
                }
                
                skipSpaces(b, pos);
                uint16_t handlerLine = readLineNumber(b, pos);
                
                // Set key trap
                if (keyIndex > 0) {
                    eventTraps.setKeyTrap(keyIndex, handlerLine);
                }
                
                return 0;
                
            } else if (eventKeyword == "TIMER") {
                // ON TIMER(interval) GOTO line
                if (t >= 0x80) ++pos; // consume token
                skipSpaces(b, pos);
                
                // Parse TIMER(interval)
                uint16_t interval = 1; // default 1 second
                if (!atEnd(b, pos) && (b[pos] == '(' || b[pos] == 0xf3)) {
                    // TIMER(interval) syntax
                    if (b[pos] == '(') ++pos;
                    else if (b[pos] == 0xf3) ++pos; // tokenized (
                    
                    skipSpaces(b, pos);
                    auto res = ev.evaluate(b, pos, env);
                    pos = res.nextPos;
                    interval = static_cast<uint16_t>(expr::ExpressionEvaluator::toInt16(res.value));
                    
                    skipSpaces(b, pos);
                    if (!atEnd(b, pos) && (b[pos] == ')' || b[pos] == 0xf4)) {
                        ++pos; // consume closing )
                    }
                }
                
                skipSpaces(b, pos);
                
                // Expect GOTO
                bool hasGoto = false;
                if (!atEnd(b, pos)) {
                    uint8_t t2 = b[pos];
                    if (t2 >= 0x80) {
                        std::string name = tok ? tok->getTokenName(t2) : std::string{};
                        if (name == "GOTO") { ++pos; hasGoto = true; }
                    } else {
                        // Try ASCII "GOTO"
                        auto save = pos;
                        std::string gotoWord;
                        for (int i = 0; i < 4 && !atEnd(b, pos); ++i) {
                            gotoWord.push_back(static_cast<char>(std::toupper(b[pos++])));
                        }
                        if (gotoWord == "GOTO") hasGoto = true; else pos = save;
                    }
                }
                
                if (!hasGoto) {
                    throw expr::BasicError(2, "Expected GOTO after ON TIMER", pos);
                }
                
                skipSpaces(b, pos);
                uint16_t handlerLine = readLineNumber(b, pos);
                
                // Set timer trap
                eventTraps.setTimerTrap(handlerLine, interval);
                
                return 0;
            }
            
            // Not an event trap - restore position and handle as computed GOTO/GOSUB
            pos = savePos;
        }
        
        // Handle traditional ON expression GOTO/GOSUB syntax
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

    uint16_t doDIM(const std::vector<uint8_t>& b, size_t& pos) {
        // DIM statement: DIM var(dim1[,dim2]...) [, var2(dim1[,dim2]...)]...
        
        while (!atEnd(b, pos)) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || b[pos] == ':') break;
            
            // Read variable name
            auto varName = readIdentifier(b, pos);
            if (varName.empty()) {
                throw expr::BasicError(2, "Expected variable name in DIM", pos);
            }
            
            skipSpaces(b, pos);
            
            // Expect opening parenthesis or bracket
            if (atEnd(b, pos)) {
                throw expr::BasicError(2, "Expected ( or [ after variable name in DIM", pos);
            }
            
            char openBracket = '\0';
            char closeBracket = '\0';
            bool isTokenized = false;
            
            // Check for ASCII parentheses/brackets or tokenized parentheses
            if (b[pos] == '(' || b[pos] == '[') {
                openBracket = static_cast<char>(b[pos]);
                closeBracket = (openBracket == '(') ? ')' : ']';
                isTokenized = false;
                ++pos; // consume opening bracket
            } else if (b[pos] == 0xf3) {  // tokenized left parenthesis
                openBracket = '(';
                closeBracket = ')';
                isTokenized = true;
                ++pos; // consume opening bracket token
            } else {
                throw expr::BasicError(2, "Expected ( or [ after variable name in DIM", pos);
            }
            
            // Parse dimension list
            std::vector<int16_t> dimensions;
            
            // Define a lambda to check for closing bracket
            auto isClosingBracket = [&](uint8_t ch) -> bool {
                if (!isTokenized) {
                    // We started with ASCII, expect ASCII closing
                    return ch == closeBracket;
                } else {
                    // We started with tokenized parenthesis, expect tokenized closing
                    return ch == 0xf4; // tokenized right parenthesis
                }
            };
            
            while (!atEnd(b, pos) && !isClosingBracket(b[pos])) {
                skipSpaces(b, pos);
                if (atEnd(b, pos)) {
                    throw expr::BasicError(2, "Missing closing bracket in DIM", pos);
                }
                
                // Evaluate dimension expression
                auto res = ev.evaluate(b, pos, env);
                pos = res.nextPos;
                
                int16_t dimSize = expr::ExpressionEvaluator::toInt16(res.value);
                if (dimSize < 0) {
                    throw expr::BasicError(5, "Illegal function call: negative dimension", pos);
                }
                
                dimensions.push_back(dimSize);
                
                skipSpaces(b, pos);
                
                // Check for comma (more dimensions)
                if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5)) { // 0xF5 is tokenized comma
                    ++pos; // consume comma
                    continue;
                }
                
                // Should be at closing bracket
                break;
            }
            
            // Expect closing bracket
            bool found_closing_bracket = false;
            if (!isTokenized) {
                // We started with ASCII, expect ASCII closing
                if (!atEnd(b, pos) && b[pos] == closeBracket) {
                    found_closing_bracket = true;
                    ++pos;
                }
            } else {
                // We started with tokenized parenthesis, expect tokenized closing
                if (!atEnd(b, pos) && b[pos] == 0xf4) { // tokenized right parenthesis
                    found_closing_bracket = true;
                    ++pos;
                }
            }
            
            if (!found_closing_bracket) {
                throw expr::BasicError(2, "Missing closing bracket in DIM", pos);
            }
            
            if (dimensions.empty()) {
                throw expr::BasicError(2, "Empty dimension list in DIM", pos);
            }
            
            // Determine array type from variable name suffix or DEFTBL
            gwbasic::ScalarType arrayType;
            char suffix = '\0';
            if (!varName.empty()) {
                char last = varName.back();
                if (last == '%' || last == '!' || last == '#' || last == '$') {
                    suffix = last;
                }
            }
            
            if (suffix) {
                arrayType = gwbasic::typeFromSuffix(suffix);
            } else {
                char firstChar = varName.empty() ? 'A' : varName[0];
                arrayType = deftbl.getDefaultFor(firstChar);
            }
            
            // Create the array
            if (!vars.createArray(varName, arrayType, dimensions)) {
                throw expr::BasicError(10, "Duplicate definition or out of memory", pos);
            }
            
            skipSpaces(b, pos);
            
            // Check for comma (more arrays)
            if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5)) { // 0xF5 is tokenized comma
                ++pos; // consume comma
                continue;
            }
            
            // End of DIM statement
            break;
        }
        
        return 0;
    }
    
    // Error handling statements
    uint16_t doERROR(const std::vector<uint8_t>& b, size_t& pos) {
        // ERROR <errorcode> - Simulate an error
        skipSpaces(b, pos);
        
        auto res = ev.evaluate(b, pos, env);
        pos = res.nextPos;
        
        uint16_t errorCode = static_cast<uint16_t>(expr::ExpressionEvaluator::toInt16(res.value));
        
        // Trigger error trap if enabled
        if (runtimeStack.hasErrorHandler()) {
            // Set up error frame
            gwbasic::ErrFrame frame{};
            frame.errCode = errorCode;
            frame.resumeLine = currentLine;
            frame.resumeTextPtr = 0;
            runtimeStack.pushErr(frame);
            
            // Jump to error handler
            return runtimeStack.getErrorHandlerLine();
        } else {
            // No error handler - should cause program termination
            throw expr::BasicError(errorCode, "Error", pos);
        }
    }
    
    uint16_t doRESUME(const std::vector<uint8_t>& b, size_t& pos) {
        // RESUME [NEXT | <line>]
        skipSpaces(b, pos);
        
        gwbasic::ErrFrame frame{};
        if (!runtimeStack.popErr(frame)) {
            throw expr::BasicError(20, "RESUME without error", pos);
        }
        
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            // RESUME - go back to line that caused error
            return frame.resumeLine;
        }
        
        // Check for NEXT keyword
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80) {
                std::string name = tok ? tok->getTokenName(t) : std::string{};
                if (name == "NEXT") {
                    ++pos;
                    // RESUME NEXT - go to next line after error
                    if (prog) {
                        auto nextIt = prog->getNextLine(frame.resumeLine);
                        if (nextIt.isValid()) {
                            return nextIt->lineNumber;
                        }
                    }
                    return 0; // End of program
                }
            } else {
                // Try ASCII "NEXT"
                auto save = pos;
                std::string next;
                for (int i = 0; i < 4 && !atEnd(b, pos); ++i) {
                    next.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
                if (next == "NEXT") {
                    // RESUME NEXT
                    if (prog) {
                        auto nextIt = prog->getNextLine(frame.resumeLine);
                        if (nextIt.isValid()) {
                            return nextIt->lineNumber;
                        }
                    }
                    return 0;
                } else {
                    pos = save;
                }
            }
        }
        
        // RESUME <line> - go to specific line
        uint16_t targetLine = readLineNumber(b, pos);
        return targetLine;
    }
    
    // Event trap control statements  
    uint16_t doKEY(const std::vector<uint8_t>& b, size_t& pos) {
        // KEY(n) ON/OFF/STOP or KEY ON/OFF
        skipSpaces(b, pos);
        
        // Check for KEY(n) syntax
        uint8_t keyIndex = 0;
        if (!atEnd(b, pos) && (b[pos] == '(' || b[pos] == 0xf3)) {
            // KEY(n) syntax - parse key index
            if (b[pos] == '(') ++pos;
            else if (b[pos] == 0xf3) ++pos; // tokenized (
            
            skipSpaces(b, pos);
            auto res = ev.evaluate(b, pos, env);
            pos = res.nextPos;
            keyIndex = static_cast<uint8_t>(expr::ExpressionEvaluator::toInt16(res.value));
            
            skipSpaces(b, pos);
            if (!atEnd(b, pos) && (b[pos] == ')' || b[pos] == 0xf4)) {
                ++pos; // consume closing )
            }
        }
        
        skipSpaces(b, pos);
        
        // Parse ON/OFF/STOP
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            std::string command;
            
            if (t >= 0x80) {
                command = tok ? tok->getTokenName(t) : std::string{};
                ++pos;
            } else {
                // Try ASCII
                auto save = pos;
                while (!atEnd(b, pos) && std::isalpha(b[pos])) {
                    command.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
                if (command.empty()) pos = save;
            }
            
            if (command == "ON") {
                if (keyIndex > 0) {
                    eventTraps.enableTrap(gwbasic::EventType::Key, keyIndex);
                } else {
                    eventTraps.enableAllTraps();
                }
            } else if (command == "OFF") {
                if (keyIndex > 0) {
                    eventTraps.disableTrap(gwbasic::EventType::Key, keyIndex);
                } else {
                    eventTraps.disableAllTraps();
                }
            } else if (command == "STOP") {
                if (keyIndex > 0) {
                    eventTraps.suspendTrap(gwbasic::EventType::Key, keyIndex);
                } else {
                    eventTraps.suspendAllTraps();
                }
            }
        }
        
        return 0;
    }
    
    uint16_t doTIMER(const std::vector<uint8_t>& b, size_t& pos) {
        // TIMER ON/OFF/STOP
        skipSpaces(b, pos);
        
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            std::string command;
            
            if (t >= 0x80) {
                command = tok ? tok->getTokenName(t) : std::string{};
                ++pos;
            } else {
                // Try ASCII
                while (!atEnd(b, pos) && std::isalpha(b[pos])) {
                    command.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
            }
            
            if (command == "ON") {
                eventTraps.enableTrap(gwbasic::EventType::Timer);
            } else if (command == "OFF") {
                eventTraps.disableTrap(gwbasic::EventType::Timer);
            } else if (command == "STOP") {
                eventTraps.suspendTrap(gwbasic::EventType::Timer);
            }
        }
        
        return 0;
    }

    uint16_t doINPUT(const std::vector<uint8_t>& b, size_t& pos) {
        // INPUT statement implementation
        // Syntax: INPUT [prompt$;] variable[,variable]...
        // or: INPUT variable[,variable]...
        
        skipSpaces(b, pos);
        
        std::string prompt;
        bool hasPrompt = false;
        bool suppressNewline = false;
        
        // Check if there's a prompt string
        if (!atEnd(b, pos)) {
            // Check if we start with a string literal
            if (b[pos] == '"') {
                // Parse string literal directly
                ++pos; // skip opening quote
                while (!atEnd(b, pos) && b[pos] != '"') {
                    prompt += static_cast<char>(b[pos++]);
                }
                if (!atEnd(b, pos) && b[pos] == '"') {
                    ++pos; // skip closing quote
                    hasPrompt = true;
                    
                    skipSpaces(b, pos);
                    
                    // Check for separator after prompt
                    if (!atEnd(b, pos) && (b[pos] == ';' || b[pos] == 0xF6 || b[pos] == ',' || b[pos] == 0xF5)) {
                        // Check separator type
                        if (b[pos] == ';' || b[pos] == 0xF6) {
                            suppressNewline = true; // Semicolon suppresses newline
                        }
                        ++pos; // consume separator
                    }
                }
            } else {
                // Save current position to backtrack if needed
                auto savePos = pos;
                
                // Try to evaluate an expression - if it's a string, it might be a prompt
                try {
                    auto res = ev.evaluate(b, pos, env);
                    
                    // Check what follows the expression
                    skipSpaces(b, pos);
                    
                    // If followed by semicolon or comma, this is a prompt
                    if (!atEnd(b, pos) && (b[pos] == ';' || b[pos] == 0xF6 || b[pos] == ',' || b[pos] == 0xF5)) {
                        // This is a prompt
                        std::visit([&](auto&& x) {
                            using T = std::decay_t<decltype(x)>;
                            if constexpr (std::is_same_v<T, expr::Str>) {
                                prompt = x.v;
                                hasPrompt = true;
                            } else {
                                // Convert numeric to string for prompt
                                if constexpr (std::is_same_v<T, expr::Int16>) prompt = std::to_string(x.v);
                                else if constexpr (std::is_same_v<T, expr::Single>) prompt = std::to_string(x.v);
                                else if constexpr (std::is_same_v<T, expr::Double>) prompt = std::to_string(x.v);
                                hasPrompt = true;
                            }
                        }, res.value);
                        
                        // Check separator type
                        if (b[pos] == ';' || b[pos] == 0xF6) {
                            suppressNewline = true; // Semicolon suppresses newline
                        }
                        ++pos; // consume separator
                    } else {
                        // Not a prompt - backtrack and treat as first variable
                        pos = savePos;
                    }
                } catch (...) {
                    // Failed to evaluate - backtrack and continue with variables
                    pos = savePos;
                }
            }
        }
        
        // Display prompt if present
        if (hasPrompt) {
            if (printCallback) {
                printCallback(prompt);
                if (!suppressNewline) {
                    printCallback(" ");
                }
            }
            // Always output to stdout as well
            std::cout << prompt;
            if (!suppressNewline) {
                std::cout << " ";
            }
        } else {
            // Default prompt
            if (printCallback) {
                printCallback("? ");
            }
            // Always output to stdout as well
            std::cout << "? ";
        }
        
        // Get list of variables to read into
        std::vector<std::string> variables;
        
        while (!atEnd(b, pos)) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || b[pos] == ':') break;
            
            auto varName = readIdentifier(b, pos);
            if (varName.empty()) {
                throw expr::BasicError(2, "Expected variable name in INPUT", pos);
            }
            
            variables.push_back(varName);
            
            skipSpaces(b, pos);
            
            // Check for comma (more variables)
            if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5)) { // 0xF5 is tokenized comma
                ++pos; // consume comma
                continue;
            }
            
            // End of variable list
            break;
        }
        
        if (variables.empty()) {
            throw expr::BasicError(2, "No variables specified in INPUT", pos);
        }
        
        // For now, we'll implement a synchronous input approach
        // In a real implementation, this would need to integrate with the event loop
        std::string inputLine;
        
        // In test mode, use default values to avoid blocking
        if (testMode) {
            inputLine = "0"; // Default input for testing
        } else if (inputCallback) {
            // Use the input callback (GUI mode)
            std::string fullPrompt;
            if (hasPrompt) {
                fullPrompt = prompt;
                if (!suppressNewline) {
                    fullPrompt += " ";
                }
            } else {
                fullPrompt = "? ";
            }
            inputLine = inputCallback(fullPrompt);
        } else {
            // Fallback to stdin (console mode)
            std::getline(std::cin, inputLine);
        }
        
        // Parse the input and assign to variables
        parseInputAndAssignVariables(inputLine, variables);
        
        return 0;
    }

private:
    void parseInputAndAssignVariables(const std::string& inputLine, const std::vector<std::string>& variables) {
        // Simple input parsing - split by commas and assign to variables
        std::vector<std::string> values;
        std::string current;
        bool inQuotes = false;
        
        for (size_t i = 0; i < inputLine.length(); ++i) {
            char ch = inputLine[i];
            
            if (ch == '"' && (i == 0 || inputLine[i-1] != '\\')) {
                inQuotes = !inQuotes;
                current += ch;
            } else if (ch == ',' && !inQuotes) {
                values.push_back(trim(current));
                current.clear();
            } else {
                current += ch;
            }
        }
        
        if (!current.empty()) {
            values.push_back(trim(current));
        }
        
        // Assign values to variables
        for (size_t i = 0; i < variables.size(); ++i) {
            std::string value;
            if (i < values.size()) {
                value = values[i];
            } else {
                value = "0"; // Default value if not enough input
            }
            
            // Determine variable type and assign
            assignInputValue(variables[i], value);
        }
    }
    
    void assignInputValue(const std::string& varName, const std::string& value) {
        // Get or create variable
        gwbasic::VarSlot& slot = vars.getOrCreate(varName);
        
        try {
            // Parse value based on variable type
            switch (slot.scalar.type) {
                case gwbasic::ScalarType::Int16: {
                    // Try to parse as integer
                    int32_t intVal = 0;
                    if (!value.empty()) {
                        try {
                            intVal = std::stoi(value);
                        } catch (...) {
                            intVal = 0; // Default on parse error
                        }
                    }
                    slot.scalar = gwbasic::Value::makeInt(static_cast<int16_t>(intVal));
                    break;
                }
                
                case gwbasic::ScalarType::Single: {
                    // Try to parse as float
                    float floatVal = 0.0f;
                    if (!value.empty()) {
                        try {
                            floatVal = std::stof(value);
                        } catch (...) {
                            floatVal = 0.0f; // Default on parse error
                        }
                    }
                    slot.scalar = gwbasic::Value::makeSingle(floatVal);
                    break;
                }
                
                case gwbasic::ScalarType::Double: {
                    // Try to parse as double
                    double doubleVal = 0.0;
                    if (!value.empty()) {
                        try {
                            doubleVal = std::stod(value);
                        } catch (...) {
                            doubleVal = 0.0; // Default on parse error
                        }
                    }
                    slot.scalar = gwbasic::Value::makeDouble(doubleVal);
                    break;
                }
                
                case gwbasic::ScalarType::String: {
                    // Remove quotes if present
                    std::string strVal = value;
                    if (strVal.length() >= 2 && strVal.front() == '"' && strVal.back() == '"') {
                        strVal = strVal.substr(1, strVal.length() - 2);
                    }
                    
                    // Allocate string in runtime heap
                    gwbasic::StrDesc sd{};
                    if (!strHeap.allocCopy(reinterpret_cast<const uint8_t*>(strVal.data()), 
                                          static_cast<uint16_t>(strVal.size()), sd)) {
                        throw expr::BasicError(7, "Out of string space", 0);
                    }
                    slot.scalar = gwbasic::Value::makeString(sd);
                    break;
                }
            }
        } catch (const std::exception&) {
            // On any error, assign default value
            switch (slot.scalar.type) {
                case gwbasic::ScalarType::Int16:
                    slot.scalar = gwbasic::Value::makeInt(0);
                    break;
                case gwbasic::ScalarType::Single:
                    slot.scalar = gwbasic::Value::makeSingle(0.0f);
                    break;
                case gwbasic::ScalarType::Double:
                    slot.scalar = gwbasic::Value::makeDouble(0.0);
                    break;
                case gwbasic::ScalarType::String: {
                    gwbasic::StrDesc sd{};
                    strHeap.allocCopy(nullptr, 0, sd); // Empty string
                    slot.scalar = gwbasic::Value::makeString(sd);
                    break;
                }
            }
        }
    }
    
    // Utility function to trim whitespace
    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\r\n");
        return str.substr(start, end - start + 1);
    }

    // READ statement implementation
    uint16_t doREAD(const std::vector<uint8_t>& b, size_t& pos) {
        // READ variable[,variable]...
        
        std::cerr << "DEBUG: doREAD() - starting, pos=" << pos << ", line has " << b.size() << " bytes" << std::endl;
        std::cerr << "DEBUG: doREAD() - tokens from pos:";
        for (size_t i = pos; i < b.size() && i < pos + 10; ++i) {
            std::cerr << " 0x" << std::hex << static_cast<int>(b[i]) << std::dec;
        }
        std::cerr << std::endl;
        
        std::vector<std::string> variables;
        
        // Parse list of variables
        while (!atEnd(b, pos)) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || b[pos] == ':') break;
            
            auto varName = readIdentifier(b, pos);
            if (varName.empty()) {
                throwBasicError(2, "Expected variable name in READ", pos);
            }
            
            variables.push_back(varName);
            
            skipSpaces(b, pos);
            
            // Check for comma (more variables)
            if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5)) { // 0xF5 is tokenized comma
                ++pos; // consume comma
                continue;
            }
            
            // End of variable list
            break;
        }
        
        if (variables.empty()) {
            throwBasicError(2, "No variables specified in READ", pos);
        }
        
        // Read values from DATA statements and assign to variables
        for (const auto& varName : variables) {
            std::cerr << "DEBUG: doREAD() - reading value for variable: " << varName << std::endl;
            gwbasic::Value value;
            if (!dataManager.readValue(value)) {
                throwBasicError(4, "Out of DATA", pos);
            }
            
            std::cerr << "DEBUG: doREAD() - value read, calling assignDataValue" << std::endl;
            // Assign the value to the variable
            assignDataValue(varName, value);
            std::cerr << "DEBUG: doREAD() - assignDataValue returned successfully" << std::endl;
        }
        
        // Skip any trailing spaces or null bytes
        while (pos < b.size() && (isSpace(b[pos]) || b[pos] == 0x00)) {
            ++pos;
        }
        
        std::cerr << "DEBUG: doREAD() - all variables processed, returning, final pos=" << pos << std::endl;
        std::cerr << "DEBUG: doREAD() - remaining bytes:";
        for (size_t i = pos; i < b.size(); ++i) {
            std::cerr << " 0x" << std::hex << static_cast<int>(b[i]) << std::dec;
        }
        std::cerr << std::endl;
        return 0;
    }
    
    // DATA statement implementation
    uint16_t doDATA(const std::vector<uint8_t>& b, size_t& pos) {
        // DATA statements are passive - they just contain data to be read
        // The DataManager handles parsing them when READ statements are executed
        // But we need to skip over the data values to avoid parsing them as statements
        
        // Skip over all data values until we reach end of line or statement separator
        while (!atEnd(b, pos) && b[pos] != ':') {
            // Skip whitespace
            while (!atEnd(b, pos) && (b[pos] == ' ' || b[pos] == '\t')) {
                pos++;
            }
            
            if (atEnd(b, pos) || b[pos] == ':') break;
            
            // Skip comma separator
            if (b[pos] == ',' || b[pos] == 0xF5) {
                pos++;
                continue;
            }
            
            // Parse and skip token value
            uint8_t token = b[pos];
            
            if (token == '"') {
                // String literal - skip to closing quote
                pos++; // Skip opening quote
                while (!atEnd(b, pos) && b[pos] != '"' && b[pos] != 0x00) {
                    pos++;
                }
                if (!atEnd(b, pos) && b[pos] == '"') {
                    pos++; // Skip closing quote
                }
            } else if (token == 0x11) {
                // Tokenized integer - skip 3 bytes total
                pos += 3;
            } else if (token == 0x1C) {
                // Single precision float - skip 5 bytes total  
                pos += 5;
            } else if (token == 0x1F) {
                // Double precision - skip 9 bytes total
                pos += 9;
            } else {
                // Unknown token, skip one byte
                pos++;
            }
        }
        
        return 0;
    }
    
    // RESTORE statement implementation
    uint16_t doRESTORE(const std::vector<uint8_t>& b, size_t& pos) {
        // RESTORE [line_number]
        
        skipSpaces(b, pos);
        
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            // RESTORE without line number - restore to beginning
            dataManager.restore();
        } else {
            // RESTORE with line number
            uint16_t lineNumber = readLineNumber(b, pos);
            dataManager.restore(lineNumber);
        }
        
        return 0;
    }

private:
    // Helper method to assign a data value to a variable
    void assignDataValue(const std::string& varName, const gwbasic::Value& value) {
        std::cerr << "DEBUG: assignDataValue() called for variable '" << varName << "'" << std::endl;
        
        // Get or create variable
        gwbasic::VarSlot& slot = vars.getOrCreate(varName);
        std::cerr << "DEBUG: assignDataValue() - variable type: " << static_cast<int>(slot.scalar.type) << std::endl;
        
        // Convert the data value to the appropriate type for the variable
        switch (slot.scalar.type) {
            case gwbasic::ScalarType::Int16: {
                int16_t intVal = 0;
                switch (value.type) {
                    case gwbasic::ScalarType::Int16:
                        intVal = value.i;
                        break;
                    case gwbasic::ScalarType::Single:
                        intVal = static_cast<int16_t>(value.f);
                        break;
                    case gwbasic::ScalarType::Double:
                        intVal = static_cast<int16_t>(value.d);
                        break;
                    case gwbasic::ScalarType::String: {
                        // Try to parse string as number
                        std::string str(reinterpret_cast<const char*>(value.s.ptr), value.s.len);
                        try {
                            intVal = static_cast<int16_t>(std::stoi(str));
                        } catch (...) {
                            intVal = 0; // Default on parse error
                        }
                        break;
                    }
                }
                slot.scalar = gwbasic::Value::makeInt(intVal);
                std::cerr << "DEBUG: assignDataValue() - assigned int value: " << intVal << std::endl;
                break;
            }
            
            case gwbasic::ScalarType::Single: {
                float floatVal = 0.0f;
                switch (value.type) {
                    case gwbasic::ScalarType::Int16:
                        floatVal = static_cast<float>(value.i);
                        break;
                    case gwbasic::ScalarType::Single:
                        floatVal = value.f;
                        break;
                    case gwbasic::ScalarType::Double:
                        floatVal = static_cast<float>(value.d);
                        break;
                    case gwbasic::ScalarType::String: {
                        // Try to parse string as number
                        std::string str(reinterpret_cast<const char*>(value.s.ptr), value.s.len);
                        try {
                            floatVal = std::stof(str);
                        } catch (...) {
                            floatVal = 0.0f; // Default on parse error
                        }
                        break;
                    }
                }
                slot.scalar = gwbasic::Value::makeSingle(floatVal);
                break;
            }
            
            case gwbasic::ScalarType::Double: {
                double doubleVal = 0.0;
                switch (value.type) {
                    case gwbasic::ScalarType::Int16:
                        doubleVal = static_cast<double>(value.i);
                        break;
                    case gwbasic::ScalarType::Single:
                        doubleVal = static_cast<double>(value.f);
                        break;
                    case gwbasic::ScalarType::Double:
                        doubleVal = value.d;
                        break;
                    case gwbasic::ScalarType::String: {
                        // Try to parse string as number
                        std::string str(reinterpret_cast<const char*>(value.s.ptr), value.s.len);
                        try {
                            doubleVal = std::stod(str);
                        } catch (...) {
                            doubleVal = 0.0; // Default on parse error
                        }
                        break;
                    }
                }
                slot.scalar = gwbasic::Value::makeDouble(doubleVal);
                break;
            }
            
            case gwbasic::ScalarType::String: {
                if (value.type == gwbasic::ScalarType::String) {
                    // Copy string value
                    gwbasic::StrDesc sd{};
                    if (!strHeap.allocCopy(value.s.ptr, value.s.len, sd)) {
                        throw expr::BasicError(7, "Out of string space", 0);
                    }
                    slot.scalar = gwbasic::Value::makeString(sd);
                } else {
                    // Convert numeric to string
                    std::string str;
                    switch (value.type) {
                        case gwbasic::ScalarType::Int16:
                            str = std::to_string(value.i);
                            break;
                        case gwbasic::ScalarType::Single:
                            str = std::to_string(value.f);
                            break;
                        case gwbasic::ScalarType::Double:
                            str = std::to_string(value.d);
                            break;
                        default:
                            str = "0";
                            break;
                    }
                    
                    gwbasic::StrDesc sd{};
                    if (!strHeap.allocCopy(reinterpret_cast<const uint8_t*>(str.c_str()), 
                                          static_cast<uint16_t>(str.length()), sd)) {
                        throw expr::BasicError(7, "Out of string space", 0);
                    }
                    slot.scalar = gwbasic::Value::makeString(sd);
                }
                break;
            }
        }
        std::cerr << "DEBUG: assignDataValue() - assignment complete" << std::endl;
    }

public:
};
