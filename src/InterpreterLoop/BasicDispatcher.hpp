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
#include <filesystem>
#include <algorithm>
#include <system_error>
#include <unordered_map>
#include <cmath>
#include <cctype>

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
#include "../Runtime/FileManager.hpp"
#include "../Runtime/UserFunctionManager.hpp"
#include "../NumericEngine/NumericEngine.hpp"
#include "../Graphics/GraphicsContext.hpp"

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
    using ScreenModeCallback = std::function<bool(int)>; // mode -> success
    using ColorCallback = std::function<bool(int, int)>; // foreground, background -> success
    using WidthCallback = std::function<bool(int)>; // columns -> success
    using GraphicsBufferCallback = std::function<uint8_t*()>; // get graphics pixel buffer
    // LOCATE callback: row, col, cursor, start, stop (1-based for row/col; -1 means "no change")
    using LocateCallback = std::function<bool(int, int, int, int, int)>;
    // CLS callback: clear the display and home cursor
    using ClsCallback = std::function<bool()>;
    // INKEY$ callback: returns a single character from keyboard buffer, or empty string if no key pressed
    using InkeyCallback = std::function<std::string()>;
    // Sound callback: play a note with frequency (Hz) for duration (ms)
    using SoundCallback = std::function<void(double, int)>;
    
    BasicDispatcher(std::shared_ptr<Tokenizer> t, std::shared_ptr<ProgramStore> p = nullptr, PrintCallback printCb = nullptr, InputCallback inputCb = nullptr, ScreenModeCallback screenCb = nullptr, ColorCallback colorCb = nullptr, GraphicsBufferCallback graphicsBufCb = nullptr, WidthCallback widthCb = nullptr, LocateCallback locateCb = nullptr, ClsCallback clsCb = nullptr, InkeyCallback inkeyCb = nullptr, SoundCallback soundCb = nullptr)
        : tok(std::move(t)), prog(std::move(p)), ev(tok), vars(&deftbl), strHeap(heapBuf, sizeof(heapBuf)), arrayManager(&strHeap), eventTraps(), dataManager(prog, tok), fileManager(), userFunctionManager(&strHeap, tok), printCallback(printCb), inputCallback(inputCb), screenModeCallback(screenCb), colorCallback(colorCb), graphicsBufferCallback(graphicsBufCb), widthCallback(widthCb), locateCallback(locateCb), clsCallback(clsCb), inkeyCallback(inkeyCb), soundCallback(soundCb), graphics() {
        // Wire up cross-references
        vars.setStringHeap(&strHeap);
        vars.setArrayManager(&arrayManager);
        // Initialize simulated DOS memory (1 MiB) and DEF SEG
        this->dosMemory.assign(1u << 20, 0u);
        this->currentSegment = 0;
        
        // Wire evaluator env to read variables from VariableTable
        env.optionBase = 0;
        env.vars.clear();
        env.getVar = [this](const std::string& name, expr::Value& out) -> bool {
            // Check for special error variables first
            if (name == "ERL") {
                // ERL returns the line number where the last error occurred
                gwbasic::ErrFrame* errFrame = runtimeStack.topErr();
                if (errFrame) {
                    out = expr::Int16{static_cast<int16_t>(errFrame->resumeLine)};
                } else {
                    out = expr::Int16{0};
                }
                return true;
            }
            if (name == "ERR") {
                // ERR returns the error code of the last error
                gwbasic::ErrFrame* errFrame = runtimeStack.topErr();
                if (errFrame) {
                    out = expr::Int16{static_cast<int16_t>(errFrame->errCode)};
                } else {
                    out = expr::Int16{0};
                }
                return true;
            }
            
            // Regular variable lookup
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
        
        // Wire function calls: handle a few special built-ins (like PEEK) before user-defined functions
        env.callFunc = [this](const std::string& name, const std::vector<expr::Value>& args, expr::Value& out) -> bool {
            // Handle PEEK(address) using current DEF SEG
            std::string upperName = name;
            std::transform(upperName.begin(), upperName.end(), upperName.begin(), [](unsigned char c){ return static_cast<char>(std::toupper(c)); });
            if (upperName == "PEEK") {
                if (args.size() != 1) {
                    this->throwBasicError(5, "Illegal function call: PEEK requires one argument", 0);
                }
                int32_t ofs = expr::ExpressionEvaluator::toInt16(args[0]);
                if (ofs < 0 || ofs > 0xFFFF) {
                    this->throwBasicError(5, "Illegal function call: address out of range", 0);
                }
                uint32_t phys = static_cast<uint32_t>(this->currentSegment) * 16u + static_cast<uint32_t>(ofs);
                if (phys >= this->dosMemory.size()) {
                    this->throwBasicError(5, "Illegal function call: address out of range", 0);
                }
                out = expr::Int16{ static_cast<int16_t>(this->dosMemory[phys]) };
                return true;
            }

            // Handle INKEY$ function
            if (upperName == "INKEY$") {
                if (args.size() != 0) {
                    this->throwBasicError(5, "Illegal function call: INKEY$ takes no arguments", 0);
                }
                // Call the INKEY$ callback if available, otherwise return empty string
                std::string key = "";
                if (this->inkeyCallback) {
                    key = this->inkeyCallback();
                }
                out = expr::Str{ key };
                return true;
            }

            // Check user-defined functions next
            if (userFunctionManager.isUserFunction(name)) {
                try {
                    std::vector<gwbasic::Value> runtimeArgs;
                    for (const auto& arg : args) runtimeArgs.push_back(this->fromExprValue(arg));
                    gwbasic::Value result;
                    if (userFunctionManager.callFunction(name, runtimeArgs, result)) { out = toExprValue(result); return true; }
                } catch (const std::exception&) {
                    // fall through
                }
            }
            return false; // Let builtin evaluator try
        };
    }

    uint16_t operator()(const std::vector<uint8_t>& tokens) {
        return operator()(tokens, 0); // default to line 0 if not specified
    }
    
    uint16_t operator()(const std::vector<uint8_t>& tokens, uint16_t currentLineNumber) {
        size_t pos = 0;
        currentLine = currentLineNumber; // store for use by statement handlers
        expr::Env envRef = env; // local view for eval
        
        // peek first significant byte
        skipSpaces(tokens, pos);
        // Some sources may include a leading tokenized line-number (0x0D LL HH); skip it defensively
        if (pos + 2 < tokens.size() && tokens[pos] == 0x0D) {
            pos += 3;
            skipSpaces(tokens, pos);
        }
        if (atEnd(tokens, pos)) return 0;

        // Debug: entry into execute loop with initial token window
        debugDumpTokens("exec:entry", tokens, pos);

        // Execute statements separated by ':' until EOL or a jump/termination happens
        while (!atEnd(tokens, pos)) {
            skipSpaces(tokens, pos);
            if (atEnd(tokens, pos)) break;
            // Stop if we reached a genuine line terminator (0x00 not belonging to a numeric token)
            if (pos < tokens.size() && isLineTerminator(tokens, pos)) break;
            // Treat both ASCII ':' and tokenized ':' as statement separators
            if (tokens[pos] == ':' || (tokens[pos] >= 0x80 && tok && tok->getTokenName(tokens[pos]) == ":")) { ++pos; continue; }
            uint8_t b = tokens[pos];
            uint16_t r = 0;
            // Debug: about to dispatch a statement at current position
            debugDumpTokens("exec:before-dispatch", tokens, pos);
            if (b >= 0x80) {
                r = handleStatement(tokens, pos);
            } else {
                r = handleLet(tokens, pos, /*implied*/true);
            }
            if (r != 0) return r; // jump or termination sentinel
            skipSpaces(tokens, pos);
            if (!atEnd(tokens, pos) && pos < tokens.size() && isLineTerminator(tokens, pos)) break;
            if (!atEnd(tokens, pos) && (tokens[pos] == ':' || (tokens[pos] >= 0x80 && tok && tok->getTokenName(tokens[pos]) == ":"))) { ++pos; continue; }
            // If not colon, continue loop which will finish if atEnd
        }
        return 0;
    }

    // Expose environment for inspection in tests
    expr::Env& environment() { return env; }

    // Access to event trap system
    gwbasic::EventTrapSystem* getEventTrapSystem() { return &eventTraps; }
    
    // Access to runtime stack for error handling
    gwbasic::RuntimeStack* getRuntimeStack() { return &runtimeStack; }
    
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
    gwbasic::FileManager fileManager;
    gwbasic::UserFunctionManager userFunctionManager;
    std::unordered_map<std::string, uint8_t> fieldToFileMap; // Maps field variable names to file numbers
    expr::Env env;
    PrintCallback printCallback;
    InputCallback inputCallback;
    ScreenModeCallback screenModeCallback;
    ColorCallback colorCallback;
    GraphicsBufferCallback graphicsBufferCallback;
    WidthCallback widthCallback;
    LocateCallback locateCallback;
    ClsCallback clsCallback;
    InkeyCallback inkeyCallback;
    SoundCallback soundCallback;
    uint16_t currentLine = 0; // Current line number being executed
    bool testMode = false; // Set to true to avoid waiting for input in tests
    GraphicsContext graphics; // Graphics drawing context
    int currentGraphicsMode_ = -1; // last set via SCREEN; -1 means unknown
    // Simulated DOS memory for PEEK/POKE and DEF SEG state
    std::vector<uint8_t> dosMemory;
    uint16_t currentSegment = 0;
    
    // DRAW statement state (mirrors GWRAM.ASM DRAW variables)
    uint8_t drawScale = 0;     // DRWSCL: scale factor for drawing (0 = no scaling)
    uint8_t drawFlags = 0;     // DRWFLG: drawing option flags (bit 7=no plot, bit 6=no move)
    uint8_t drawAngle = 0;     // DRWANG: rotation angle (0-3 for 0°, 90°, 180°, 270°)

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

    gwbasic::Value fromExprValue(const expr::Value& v) {
        return std::visit([this](auto&& x) -> gwbasic::Value {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Int16>) {
                return gwbasic::Value::makeInt(x.v);
            } else if constexpr (std::is_same_v<T, expr::Single>) {
                return gwbasic::Value::makeSingle(x.v);
            } else if constexpr (std::is_same_v<T, expr::Double>) {
                return gwbasic::Value::makeDouble(x.v);
            } else if constexpr (std::is_same_v<T, expr::Str>) {
                // Allocate string in runtime heap
                gwbasic::StrDesc sd{};
                if (!strHeap.allocCopy(reinterpret_cast<const uint8_t*>(x.v.data()), static_cast<uint16_t>(x.v.size()), sd)) {
                    throwBasicError(7, "Out of string space", 0);
                }
                return gwbasic::Value::makeString(sd);
            }
        }, v);
    }

    static bool atEnd(const std::vector<uint8_t>& b, size_t pos) {
        // Only treat buffer exhaustion as end. 0x00 sentinel handled explicitly
        // in the execution loop to avoid interfering with numeric token data.
        return pos >= b.size();
    }
    static bool isLineTerminator(const std::vector<uint8_t>& b, size_t pos) {
        return b[pos] == 0x00;
    }
    static bool isSpace(uint8_t c) { return c==' '||c=='\t'||c=='\r'||c=='\n'; }
    static void skipSpaces(const std::vector<uint8_t>& b, size_t& pos) { while (pos < b.size() && isSpace(b[pos])) ++pos; }
    
    // Helper: match either ASCII symbol or its tokenized form by name
    bool isSymbolAt(const std::vector<uint8_t>& b, size_t pos, char ascii, const char* tokenName) const {
        if (pos >= b.size()) return false;
        if (b[pos] == static_cast<uint8_t>(ascii)) return true;
        if (b[pos] >= 0x80 && tok) return tok->getTokenName(b[pos]) == tokenName;
        return false;
    }
    
    // Helper: consume symbol if present
    bool consumeSymbol(const std::vector<uint8_t>& b, size_t& pos, char ascii, const char* tokenName) const {
        if (isSymbolAt(b, pos, ascii, tokenName)) { ++pos; return true; }
        return false;
    }

    // Helper: consume a keyword that may be tokenized or ASCII letters. Case-insensitive for ASCII.
    bool consumeKeyword(const std::vector<uint8_t>& b, size_t& pos, const char* keyword) const {
        if (atEnd(b, pos)) return false;
        // Tokenized path
        if (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == keyword) { ++pos; return true; }
        // ASCII fallback
        size_t p = pos;
        size_t klen = std::strlen(keyword);
        for (size_t i = 0; i < klen; ++i) {
            if (p >= b.size()) { p = pos; return false; }
            char c = static_cast<char>(b[p]);
            if (!std::isalpha(static_cast<unsigned char>(c))) { p = pos; return false; }
            if (std::toupper(c) != std::toupper(keyword[i])) { p = pos; return false; }
            ++p;
        }
        // Ensure next char isn't an identifier char to avoid partial match (e.g., STEPP)
        if (p < b.size()) {
            char c = static_cast<char>(b[p]);
            if (std::isalnum(static_cast<unsigned char>(c))) { return false; }
        }
        pos = p; return true;
    }

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

    // Helper method to catch and re-throw BasicError with line number context
    template<typename Func>
    auto catchAndRethrowWithLineNumber(Func&& func) -> decltype(func()) {
        try {
            return func();
        } catch (const expr::BasicError& e) {
            // Re-throw with line number included
            throwBasicError(e.code, e.what(), e.position);
            throw; // This line should never be reached, but keeps compiler happy
        }
    }

    // Debug helper: dump a window of upcoming tokens to a log file for diagnostics
    void debugDumpTokens(const char* tag, const std::vector<uint8_t>& b, size_t pos, size_t count = 24) const {
        try {
            std::ofstream ofs("/tmp/gwbasic_debug.log", std::ios::app);
            if (!ofs) return;
            ofs << tag << " line=" << currentLine << " pos=" << pos << " bytes:";
            size_t end = std::min(b.size(), pos + count);
            for (size_t i = pos; i < end; ++i) {
                ofs << ' ' << std::hex << std::showbase << static_cast<int>(b[i]) << std::dec;
            }
            ofs << " | names:";
            for (size_t i = pos; i < end; ++i) {
                uint8_t c = b[i];
                if (c == 0x00) { ofs << " [EOL]"; break; }
                if (c >= 0x80 && tok) {
                    ofs << " [" << tok->getTokenName(c) << "]";
                } else if (c == '"') {
                    ofs << " [";
                    ofs << '"';
                    ofs << '"';
                    ofs << "]";
                } else if (c >= ' ' && c < 0x7f) {
                    ofs << ' ' << static_cast<char>(c);
                } else if (c == 0x11) {
                    if (i + 2 < b.size()) {
                        uint16_t v = static_cast<uint16_t>(b[i+1]) | (static_cast<uint16_t>(b[i+2]) << 8);
                        ofs << " [INT:" << v << "]"; i += 2;
                    } else {
                        ofs << " [INT:?]";
                    }
                } else if (c == 0x1D) {
                    ofs << " [SINGLE]"; i += 4;
                } else if (c == 0x1F) {
                    ofs << " [DOUBLE]"; i += 8;
                }
            }
            ofs << '\n';
        } catch (...) {
            // ignore
        }
    }

    uint16_t handleStatement(const std::vector<uint8_t>& b, size_t& pos) {
        if (pos >= b.size()) return 0;
        // Debug: entering statement handler with upcoming tokens
        debugDumpTokens("handleStmt:entry", b, pos);
        uint8_t t = b[pos++];
        // Debug: show the just-consumed token as well
        if (pos > 0) {
            size_t back = pos - 1;
            debugDumpTokens("handleStmt:token", b, back);
        }
    // Handle extended statements (0xFE <index>)
    if (t == 0xFE) { // EXTENDED_STATEMENT_PREFIX
            if (pos >= b.size()) return 0;
            uint8_t idx = b[pos++];
            // Debug: show extended statement index token position
            if (pos > 0) {
                size_t back = pos - 1;
                debugDumpTokens("handleStmt:ext-index", b, back);
            }
            // Minimal mapping for the few extended statements we support here
            switch (idx) {
                case 0x00: // FILES
                    return doFILES(b, pos);
                case 0x01: // FIELD
                    return doFIELD(b, pos);
                case 0x02: // SYSTEM
                    return 0xFFFF; // terminate program
                case 0x03: // NAME
                    return doNAME(b, pos);
                case 0x04: // LSET
                    return doLSET(b, pos);
                case 0x05: // RSET
                    return doRSET(b, pos);
                case 0x06: // KILL
                    return doKILL(b, pos);
                case 0x07: // PUT
                    return doPUT(b, pos);
                case 0x08: // GET
                    return doGET(b, pos);
                case 0x10: // CIRCLE
                    return doCIRCLE(b, pos);
                case 0x11: // DRAW
                    return doDRAW(b, pos);
                case 0x12: // PLAY
                    return doPLAY(b, pos);
                case 0x13: // TIMER (extended statement)
                    return doTIMER(b, pos);
                default:
                    return 0; // unsupported extended statement -> no-op
            }
        }

        std::string name = tok ? tok->getTokenName(t) : std::string{};
        
        if (name == "PRINT") return doPRINT(b, pos);
        if (name == "END" || name == "STOP") return 0xFFFF;
        if (name == "INPUT") {
            return doINPUT(b, pos);
        }
        if (name == "LET") return handleLet(b, pos, false);
    if (name == "DIM") return doDIM(b, pos);
    if (name == "DEF") return doDEF(b, pos);
    if (name == "DEFINT") return doDEFType(b, pos, gwbasic::ScalarType::Int16);
    if (name == "DEFSNG") return doDEFType(b, pos, gwbasic::ScalarType::Single);
    if (name == "DEFDBL") return doDEFType(b, pos, gwbasic::ScalarType::Double);
    if (name == "DEFSTR") return doDEFType(b, pos, gwbasic::ScalarType::String);
        if (name == "READ") return doREAD(b, pos);
        if (name == "DATA") return doDATA(b, pos);
        if (name == "RESTORE") return doRESTORE(b, pos);
        if (name == "REM") return doREM(b, pos);
        if (name == "IF") return doIF(b, pos);
        if (name == "GOTO") return doGOTO(b, pos);
        if (name == "FOR") return doFOR(b, pos);
        if (name == "NEXT") return doNEXT(b, pos);
        if (name == "GOSUB") return doGOSUB(b, pos);
        if (name == "RETURN") return doRETURN(b, pos);
        if (name == "ON") return doON(b, pos);
        if (name == "LOAD") return doLOAD(b, pos);
        if (name == "SAVE") return doSAVE(b, pos);
        if (name == "OPEN") return doOPEN(b, pos);
    if (name == "CLOSE") return doCLOSE(b, pos);
    if (name == "POKE") return doPOKE(b, pos);
        if (name == "ERROR") return doERROR(b, pos);
        if (name == "RESUME") return doRESUME(b, pos);
        if (name == "KEY") return doKEY(b, pos);
        if (name == "TIMER") return doTIMER(b, pos);
        if (name == "SCREEN") return doSCREEN(b, pos);
        if (name == "COLOR") return doCOLOR(b, pos);
        if (name == "WIDTH") return doWIDTH(b, pos);
        if (name == "CLS") return doCLS(b, pos);
        if (name == "PSET") return doPSET(b, pos);
        if (name == "PRESET") return doPRESET(b, pos);
        if (name == "LINE") return doLINE(b, pos);
        if (name == "LOCATE") return doLOCATE(b, pos);
        return 0;
    }

    // FILES [filespec$]
    uint16_t doFILES(const std::vector<uint8_t>& b, size_t& pos) {
        using namespace std;
        namespace fs = std::filesystem;

        // Parse optional filespec expression
        skipSpaces(b, pos);
        string filespec;
        bool hasArg = false;
        if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            hasArg = true;
            auto specRes = ev.evaluate(b, pos, env);
            pos = specRes.nextPos;
            visit([&](auto&& x){
                using T = decay_t<decltype(x)>;
                if constexpr (is_same_v<T, expr::Str>) {
                    filespec = x.v;
                } else {
                    // In GW-BASIC, filespec is a string; reject non-strings
                    this->throwBasicError(13, "Type mismatch: filespec must be string", pos);
                }
            }, specRes.value);
        }

        if (!hasArg) {
            filespec = "*.*"; // DOS default
        }

        // Normalize path separators to '/'
        for (char& c : filespec) { if (c == '\\') c = '/'; }

        // Split directory and pattern
        string dirPath = ".";
        string pattern = filespec;
        auto slashPos = filespec.find_last_of('/');
        if (slashPos != string::npos) {
            dirPath = filespec.substr(0, slashPos);
            if (dirPath.empty()) dirPath = "/";
            pattern = filespec.substr(slashPos + 1);
            if (pattern.empty()) pattern = "*.*";
        }

        auto toUpper = [](string s){
            for (auto& ch : s) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));
            return s;
        };

        // Simple DOS-style wildcard matcher: * and ?
        function<bool(const string&, const string&)> match = [&](const string& pat, const string& name) -> bool {
            const string P = toUpper(pat);
            const string N = toUpper(name);
            size_t p = 0, n = 0, star = string::npos, matchIdx = 0;
            while (n < N.size()) {
                if (p < P.size() && (P[p] == '?' || P[p] == N[n])) { ++p; ++n; continue; }
                if (p < P.size() && P[p] == '*') { star = p++; matchIdx = n; continue; }
                if (star != string::npos) { p = star + 1; n = ++matchIdx; continue; }
                return false;
            }
            while (p < P.size() && P[p] == '*') ++p;
            return p == P.size();
        };

        vector<string> names;
        try {
            for (const auto& de : fs::directory_iterator(dirPath)) {
                string name = de.path().filename().string();
                if (match(pattern, name)) {
                    names.push_back(name);
                }
            }
        } catch (const std::exception&) {
            // Directory doesn't exist or cannot be iterated
            this->throwBasicError(53, "File not found", pos);
        }

        std::sort(names.begin(), names.end(), [&](const string& a, const string& b){
            return toUpper(a) < toUpper(b);
        });

        // Emit results, one per line
        string out;
        for (const auto& n : names) { out += n; out += '\n'; }
        if (this->printCallback) this->printCallback(out);
        return 0;
    }

    // KILL filename$
    uint16_t doKILL(const std::vector<uint8_t>& b, size_t& pos) {
        using namespace std;
        namespace fs = std::filesystem;

        // Parse filename expression (required)
        skipSpaces(b, pos);
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            this->throwBasicError(2, "Missing filename in KILL statement", pos);
        }

        auto filenameRes = ev.evaluate(b, pos, env);
        pos = filenameRes.nextPos;
        
        string filename;
        visit([&](auto&& x){
            using T = decay_t<decltype(x)>;
            if constexpr (is_same_v<T, expr::Str>) {
                filename = x.v;
            } else {
                // In GW-BASIC, filename must be a string
                this->throwBasicError(13, "Type mismatch: filename must be string", pos);
            }
        }, filenameRes.value);

        if (filename.empty()) {
            this->throwBasicError(2, "Empty filename in KILL statement", pos);
        }

        // Normalize path separators
        for (char& c : filename) { if (c == '\\') c = '/'; }

        // Attempt to delete the file
        try {
            if (!fs::exists(filename)) {
                this->throwBasicError(53, "File not found", pos);
            }
            
            if (fs::is_directory(filename)) {
                this->throwBasicError(70, "Permission denied", pos);
            }
            
            if (!fs::remove(filename)) {
                this->throwBasicError(70, "Permission denied", pos);
            }
            
        } catch (const fs::filesystem_error&) {
            this->throwBasicError(70, "Permission denied", pos);
        } catch (const std::exception&) {
            this->throwBasicError(70, "Permission denied", pos);
        }

        return 0;
    }

    // NAME oldname$ AS newname$
    uint16_t doNAME(const std::vector<uint8_t>& b, size_t& pos) {
        using namespace std;
        namespace fs = std::filesystem;

        // Parse oldname expression (required)
        skipSpaces(b, pos);
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            this->throwBasicError(2, "Missing filename in NAME statement", pos);
        }

        auto oldnameRes = ev.evaluate(b, pos, env);
        pos = oldnameRes.nextPos;
        
        string oldname;
        visit([&](auto&& x){
            using T = decay_t<decltype(x)>;
            if constexpr (is_same_v<T, expr::Str>) {
                oldname = x.v;
            } else {
                // In GW-BASIC, filename must be a string
                this->throwBasicError(13, "Type mismatch: filename must be string", pos);
            }
        }, oldnameRes.value);

        skipSpaces(b, pos);
        
        // Expect AS keyword (tokenized or ASCII)
        bool hasAs = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80 && tok && tok->getTokenName(t) == "AS") {
                ++pos;
                hasAs = true;
            } else if (t >= 0x20 && t < 0x80) {
                // Try ASCII "AS"
                auto save = pos;
                string asWord;
                for (int i = 0; i < 2 && !atEnd(b, pos) && b[pos] >= 0x20 && b[pos] < 0x80; ++i) {
                    asWord.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
                if (asWord == "AS") hasAs = true; else pos = save;
            }
        }
        
        if (!hasAs) {
            this->throwBasicError(2, "Expected AS in NAME statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse newname expression (required)
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            this->throwBasicError(2, "Missing new filename in NAME statement", pos);
        }

        auto newnameRes = ev.evaluate(b, pos, env);
        pos = newnameRes.nextPos;
        
        string newname;
        visit([&](auto&& x){
            using T = decay_t<decltype(x)>;
            if constexpr (is_same_v<T, expr::Str>) {
                newname = x.v;
            } else {
                // In GW-BASIC, filename must be a string
                this->throwBasicError(13, "Type mismatch: filename must be string", pos);
            }
        }, newnameRes.value);

        if (oldname.empty()) {
            this->throwBasicError(2, "Empty old filename in NAME statement", pos);
        }
        
        if (newname.empty()) {
            this->throwBasicError(2, "Empty new filename in NAME statement", pos);
        }

        // Normalize path separators
        for (char& c : oldname) { if (c == '\\') c = '/'; }
        for (char& c : newname) { if (c == '\\') c = '/'; }

        // Attempt to rename the file
        try {
            if (!fs::exists(oldname)) {
                this->throwBasicError(53, "File not found", pos);
            }
            
            if (fs::exists(newname)) {
                this->throwBasicError(58, "File already exists", pos);
            }
            
            fs::rename(oldname, newname);
            
        } catch (const fs::filesystem_error& e) {
            // Map filesystem errors to appropriate GW-BASIC error codes
            auto ec = e.code();
            if (ec == errc::no_such_file_or_directory) {
                this->throwBasicError(53, "File not found", pos);
            } else if (ec == errc::file_exists || ec == errc::directory_not_empty) {
                this->throwBasicError(58, "File already exists", pos);
            } else if (ec == errc::permission_denied || ec == errc::operation_not_permitted) {
                this->throwBasicError(70, "Permission denied", pos);
            } else {
                this->throwBasicError(70, "Permission denied", pos);
            }
        } catch (const std::exception&) {
            this->throwBasicError(70, "Permission denied", pos);
        }

        return 0;
    }

    uint16_t doPRINT(const std::vector<uint8_t>& b, size_t& pos) {
        debugDumpTokens("doPRINT:entry", b, pos);
        // Check for PRINT# (file output) syntax first
        skipSpaces(b, pos);
        uint8_t fileNumber = 0; // 0 means console output
        bool isFileOutput = false;
        
    // Check if starts with # (file number) - ASCII or tokenized
        if (!atEnd(b, pos) && (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            ++pos; // consume #
            skipSpaces(b, pos);
            
            // Parse file number
            auto fileNumRes = ev.evaluate(b, pos, env);
            pos = fileNumRes.nextPos;
            
            fileNumber = static_cast<uint8_t>(expr::ExpressionEvaluator::toInt16(fileNumRes.value));
            isFileOutput = true;
            
            // Validate file number and check if open for output
            if (!gwbasic::FileManager::isValidFileNumber(fileNumber)) {
                throwBasicError(52, "Bad file number", pos);
            }
            
            if (!fileManager.isFileOpen(fileNumber)) {
                throwBasicError(62, "Input past end", pos);
            }
            
            auto* fileHandle = fileManager.getFile(fileNumber);
            if (!fileHandle || (fileHandle->mode != gwbasic::FileMode::OUTPUT && 
                              fileHandle->mode != gwbasic::FileMode::APPEND)) {
                throwBasicError(54, "Bad file mode", pos);
            }
            
            skipSpaces(b, pos);
            
            // Expect comma after file number (ASCII or tokenized)
            if (!atEnd(b, pos) && (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ","))) {
                ++pos; // consume comma
                skipSpaces(b, pos);
            } else {
                throwBasicError(2, "Expected comma after file number in PRINT#", pos);
            }
        }
        
        // Check for PRINT USING syntax
        if (!atEnd(b, pos)) {
            // Check if the next token is USING
            if (tok) {
                std::string nextTokenName = tok->getTokenName(b[pos]);
                if (nextTokenName == "USING") {
                    return doPRINTUSING(b, pos, fileNumber, isFileOutput);
                }
            }
        }
        
        // Regular PRINT statement
        bool trailingSemicolon = false; // controls newline suppression only if last token is ';'
        std::string output;
        
        while (!atEnd(b, pos)) {
            // Respect line terminator explicitly
            if (pos < b.size() && b[pos] == 0x00) break;
            skipSpaces(b, pos);
            if (atEnd(b, pos)) break;
            if (pos < b.size() && b[pos] == 0x00) break; // after skipping spaces
            // Separators
            // Semicolon token or ASCII
            if (b[pos] == ';' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ";")) {
                trailingSemicolon = true;
                ++pos;
                debugDumpTokens("doPRINT:found-semicolon", b, pos);
                continue;
            }
            // Comma token or ASCII
            if (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")) {
                output += "\t";
                trailingSemicolon = false;
                ++pos;
                debugDumpTokens("doPRINT:found-comma", b, pos);
                continue;
            }
            // Colon token or ASCII: end of PRINT list
            if (b[pos] == ':' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ":")) { ++pos; break; }
            // Expression
            debugDumpTokens("doPRINT:before-expr", b, pos);
            auto res = catchAndRethrowWithLineNumber([&]() { 
                return ev.evaluate(b, pos, env); 
            });
            pos = res.nextPos;
            debugDumpTokens("doPRINT:after-expr", b, pos);
            std::visit([&](auto&& x){ 
                using T = std::decay_t<decltype(x)>; 
                if constexpr (std::is_same_v<T, expr::Str>) output += x.v; 
                else if constexpr (std::is_same_v<T, expr::Int16>) output += std::to_string(x.v); 
                else if constexpr (std::is_same_v<T, expr::Single>) output += std::to_string(x.v); 
                else if constexpr (std::is_same_v<T, expr::Double>) output += std::to_string(x.v); 
            }, res.value);
            trailingSemicolon = false;
        }
        if (!trailingSemicolon) output += "\n";
        
        // Send output via callback or file
        if (isFileOutput) {
            // Write to file
            if (!fileManager.writeData(fileNumber, output)) {
                throwBasicError(61, "Disk full", pos);
            }
        } else {
            // Send to console/GUI via callback
            if (printCallback) {
                printCallback(output);
            }
        }
        
        return 0;
    }

    uint16_t doPRINTUSING(const std::vector<uint8_t>& b, size_t& pos, uint8_t fileNumber = 0, bool isFileOutput = false) {
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
        
        // Expect semicolon after format string (tokenized or ASCII)
        if (!atEnd(b, pos) && (b[pos] == ';' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ";"))) {
            ++pos;
        } else {
            throwBasicError(2, "Expected ';' after USING format string", pos);
        }
        
    std::string output;
    bool trailingSemicolon = false;
        
        // Process the value list
        while (!atEnd(b, pos)) {
            if (pos < b.size() && b[pos] == 0x00) break;
            skipSpaces(b, pos);
            if (atEnd(b, pos)) break;
            if (pos < b.size() && b[pos] == 0x00) break;
            
            // Handle separators
            if (b[pos] == ';' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ";")) {  // semicolon token or ASCII
                trailingSemicolon = true; 
                ++pos; 
                continue; 
            }
            if (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")) {  // comma token or ASCII
                // For PRINT USING, comma usually means next field, 
                // but we'll treat it as continuation
                ++pos; 
                trailingSemicolon = false; 
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
            trailingSemicolon = false;
            
            skipSpaces(b, pos);
            if (!atEnd(b, pos) && (b[pos] == ':' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ":"))) { ++pos; break; }
            if (!atEnd(b, pos) && (b[pos] == ';' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ";"))) { trailingSemicolon = true; ++pos; }
            if (!atEnd(b, pos) && (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ","))) { ++pos; trailingSemicolon = false; }
        }
        
        if (!trailingSemicolon) output += "\n";
        
        // Send output via callback or file
        if (isFileOutput) {
            // Write to file
            if (!fileManager.writeData(fileNumber, output)) {
                throwBasicError(61, "Disk full", pos);
            }
        } else {
            // Send to console/GUI via callback
            if (printCallback) {
                printCallback(output);
            }
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

    // OPEN "filename" FOR mode AS #filenumber
    uint16_t doOPEN(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        
        // Parse filename (string expression)
        auto filenameRes = ev.evaluate(b, pos, env);
        pos = filenameRes.nextPos;
        
        std::string filename;
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Str>) {
                filename = x.v;
            } else {
                throwBasicError(13, "Type mismatch: filename must be string", pos);
            }
        }, filenameRes.value);
        
        skipSpaces(b, pos);
        
        // Expect FOR keyword
        bool hasFor = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t == 0x81) { // TOKEN_FOR
                ++pos; 
                hasFor = true;
            } else if (t >= 0x20 && t < 0x80) {
                // Try ASCII "FOR"
                auto save = pos;
                std::string forWord;
                for (int i = 0; i < 3 && !atEnd(b, pos) && b[pos] >= 0x20 && b[pos] < 0x80; ++i) {
                    forWord.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
                if (forWord == "FOR") hasFor = true; else pos = save;
            }
        }
        
        if (!hasFor) {
            throwBasicError(2, "Expected FOR in OPEN statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse mode keyword (INPUT, OUTPUT, APPEND, RANDOM)
        std::string modeStr;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t == 0x84) { // TOKEN_INPUT
                modeStr = "INPUT";
                ++pos;
            } else if (t >= 0x20 && t < 0x80) {
                // Parse ASCII mode keyword - read until space or non-alpha
                std::string word;
                while (!atEnd(b, pos) && b[pos] >= 0x20 && b[pos] < 0x80 && 
                       (std::isalpha(b[pos]) || std::isdigit(b[pos]))) {
                    char c = static_cast<char>(std::toupper(b[pos++]));
                    word.push_back(c);
                    // Stop if we've reached potential keywords like AS
                    if (word.length() >= 2 && word.substr(word.length() - 2) == "AS") {
                        // Check if this is just "AS" or ends with AS (like "OUTPUTAS")
                        if (word == "AS") {
                            pos -= 2; // Back up to parse AS separately
                            word = "";
                            break;
                        } else if (word.length() > 2) {
                            // This might be something like "OUTPUTAS" - split it
                            std::string prefix = word.substr(0, word.length() - 2);
                            if (prefix == "OUTPUT" || prefix == "APPEND" || prefix == "RANDOM") {
                                pos -= 2; // Back up so AS can be parsed separately
                                word = prefix;
                                break;
                            }
                        }
                    }
                }
                if (word == "OUTPUT" || word == "INPUT" || word == "APPEND" || word == "RANDOM") {
                    modeStr = word;
                } else if (!word.empty()) {
                    throwBasicError(2, "Invalid file mode: " + word, pos);
                }
            } else {
                throwBasicError(2, "Expected file mode (INPUT, OUTPUT, APPEND, RANDOM)", pos);
            }
        }
        
        skipSpaces(b, pos);
        
        // Convert mode string to FileMode
        gwbasic::FileMode mode;
        
        if (modeStr == "INPUT") {
            mode = gwbasic::FileMode::INPUT;
        } else if (modeStr == "OUTPUT") {
            mode = gwbasic::FileMode::OUTPUT;
        } else if (modeStr == "APPEND") {
            mode = gwbasic::FileMode::APPEND;
        } else if (modeStr == "RANDOM") {
            mode = gwbasic::FileMode::RANDOM;
        } else {
            throwBasicError(5, "Illegal function call: invalid file mode '" + modeStr + "'", pos);
        }
        
        skipSpaces(b, pos);
        
        // Expect AS keyword (tokenized or ASCII)
        bool hasAs = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80 && tok && tok->getTokenName(t) == "AS") {
                ++pos;
                hasAs = true;
            } else if (t >= 0x20 && t < 0x80) {
                // Try ASCII "AS"
                auto save = pos;
                std::string asWord;
                for (int i = 0; i < 2 && !atEnd(b, pos) && b[pos] >= 0x20 && b[pos] < 0x80; ++i) {
                    asWord.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
                if (asWord == "AS") hasAs = true; else pos = save;
            }
        }
        
        if (!hasAs) {
            throwBasicError(2, "Expected AS in OPEN statement", pos);
        }
        
        skipSpaces(b, pos);
        
    // Expect # for file number (either ASCII '#' or tokenized operator)
    if (atEnd(b, pos) || !(b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            throwBasicError(2, "Expected # in OPEN statement", pos);
        }
        ++pos; // consume #
        
        skipSpaces(b, pos);
        
        // Parse file number
        auto fileNumRes = ev.evaluate(b, pos, env);
        pos = fileNumRes.nextPos;
        
        uint8_t fileNumber = static_cast<uint8_t>(expr::ExpressionEvaluator::toInt16(fileNumRes.value));
        
        // Validate file number
        if (!gwbasic::FileManager::isValidFileNumber(fileNumber)) {
            throwBasicError(52, "Bad file number", pos);
        }
        
        // Check for LEN parameter for random access files
        size_t recordLength = 128; // Default record length
        if (mode == gwbasic::FileMode::RANDOM) {
            skipSpaces(b, pos);
            
            // Check for LEN keyword - make this more flexible
            if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                bool foundLen = false;
                auto savePos = pos;
                
                // Check for tokenized LEN (0x11)
                if (b[pos] == 0x11) {
                    foundLen = true;
                    ++pos;
                } else if (b[pos] >= 0x20 && b[pos] < 0x80) {
                    // Try to read ASCII "LEN" - skip any spaces that might be in between
                    std::string word;
                    auto tempPos = pos;
                    
                    // Read the next word, skipping spaces
                    while (!atEnd(b, tempPos) && (std::isalpha(b[tempPos]) || std::isspace(b[tempPos]))) {
                        if (std::isalpha(b[tempPos])) {
                            word.push_back(static_cast<char>(std::toupper(b[tempPos])));
                        }
                        tempPos++;
                        if (word.length() >= 3) break; // Stop after reading enough for "LEN"
                    }
                    
                    if (word == "LEN") {
                        foundLen = true;
                        pos = tempPos; // Update position past the LEN word
                    }
                }
                
                if (foundLen) {
                    skipSpaces(b, pos);
                    
                    // Look for = sign - be more flexible about what we accept
                    // Note: = can be tokenized as different values depending on context
                    bool foundEquals = false;
                    if (!atEnd(b, pos)) {
                        if (b[pos] == '=' || b[pos] == 0xD1 || b[pos] == 0xD2 || b[pos] == 0xE5) {
                            foundEquals = true;
                            ++pos; // consume =
                        }
                    }
                    
                    if (foundEquals) {
                        skipSpaces(b, pos);
                        
                        // Parse record length
                        auto lenRes = ev.evaluate(b, pos, env);
                        pos = lenRes.nextPos;
                        int len = expr::ExpressionEvaluator::toInt16(lenRes.value);
                        
                        if (len <= 0 || len > 32767) {
                            throwBasicError(5, "Illegal function call: invalid record length", pos);
                        }
                        
                        recordLength = static_cast<size_t>(len);
                    } else {
                        // If we found LEN but no =, it's an error
                        throwBasicError(2, "Expected = after LEN", pos);
                    }
                } else {
                    // Restore position if we didn't find LEN
                    pos = savePos;
                }
            }
        }
        
        // Try to open the file
        if (!fileManager.openFile(fileNumber, filename, mode, recordLength)) {
            // Determine appropriate error message based on mode and filename
            if (mode == gwbasic::FileMode::INPUT) {
                throwBasicError(53, "File not found", pos);
            } else {
                throwBasicError(70, "Permission denied", pos);
            }
        }
        
        return 0;
    }

    // CLOSE [#filenumber[,#filenumber]...] or CLOSE (close all)
    uint16_t doCLOSE(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        
        // If no arguments, close all files
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            fileManager.closeAll();
            return 0;
        }
        
        // Parse file number list
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) break;
            
            // Optional # prefix (ASCII or tokenized)
            if (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#")) {
                ++pos; // consume #
                skipSpaces(b, pos);
            }
            
            // Parse file number
            auto fileNumRes = ev.evaluate(b, pos, env);
            pos = fileNumRes.nextPos;
            
            uint8_t fileNumber = static_cast<uint8_t>(expr::ExpressionEvaluator::toInt16(fileNumRes.value));
            
            // Validate and close file
            if (!gwbasic::FileManager::isValidFileNumber(fileNumber)) {
                throwBasicError(52, "Bad file number", pos);
            }
            
            if (!fileManager.closeFile(fileNumber)) {
                // File was not open - this is not an error in GW-BASIC
                // Just continue silently
            }
            
            skipSpaces(b, pos);
            
            // Check for comma (more file numbers)
            if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5)) { // 0xF5 is tokenized comma
                ++pos; // consume comma
                continue;
            }
            
            // End of file number list
            break;
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
        if (name.empty()) throwBasicError(2, "Syntax error", pos);
        skipSpaces(b, pos);
        
        // Check if this is an array assignment: VAR(indices) = expr
        if (!atEnd(b, pos) && (isSymbolAt(b, pos, '(', "(") || b[pos] == '[')) {
            // Array assignment
            char openBracket = '\0';
            char closeBracket = '\0';
            bool parenTokenized = false;
            
            if (b[pos] == '(' || b[pos] == '[') {
                openBracket = static_cast<char>(b[pos]);
                closeBracket = (openBracket == '(') ? ')' : ']';
                parenTokenized = false;
                ++pos; // consume opening bracket
            } else if (isSymbolAt(b, pos, '(', "(")) {  // tokenized left parenthesis
                openBracket = '(';
                closeBracket = ')';
                parenTokenized = true;
                ++pos; // consume opening bracket token
            }
            
            // Parse indices
            std::vector<int32_t> indices;
            
            // Define a lambda to check for closing bracket
            auto isClosingBracket = [&](uint8_t ch) -> bool {
                if (!parenTokenized) {
                    // We started with ASCII, expect ASCII closing
                    return ch == closeBracket;
                } else {
                    // We started with tokenized parenthesis, expect tokenized closing
                    return (ch == static_cast<uint8_t>(')')) || (ch >= 0x80 && tok && tok->getTokenName(ch) == ")");
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
                if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) {
                    ++pos; // consume comma
                    continue;
                }
                
                break;
            }
            
            // Expect closing bracket
            bool found_closing_bracket = false;
            if (!parenTokenized) {
                // We started with ASCII, expect ASCII closing
                if (!atEnd(b, pos) && b[pos] == closeBracket) {
                    found_closing_bracket = true;
                    ++pos;
                }
            } else {
                // We started with tokenized parenthesis, expect tokenized closing
                if (!atEnd(b, pos) && isSymbolAt(b, pos, ')', ")")) { // tokenized right parenthesis
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

    // Helper: parse letter ranges for DEFxxx (e.g., A-C, M, X-Z)
    void parseLetterRanges(const std::vector<uint8_t>& b, size_t& pos, const std::function<void(char,char)>& applyRange) {
        auto parseOne = [&](char& from, char& to)->bool {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || !std::isalpha(b[pos])) return false;
            from = static_cast<char>(std::toupper(b[pos++]));
            skipSpaces(b, pos);
            // Accept ASCII '-' or tokenized minus operator
            bool hasMinus = false;
            if (!atEnd(b, pos)) {
                if (b[pos] == '-') {
                    hasMinus = true;
                } else if (b[pos] >= 0x80 && tok) {
                    std::string tname = tok->getTokenName(b[pos]);
                    if (tname == "-") hasMinus = true;
                }
            }
            if (hasMinus) {
                ++pos; skipSpaces(b, pos);
                if (atEnd(b, pos) || !std::isalpha(b[pos])) throw expr::BasicError(2, "Bad DEF type range", pos);
                to = static_cast<char>(std::toupper(b[pos++]));
            } else {
                to = from;
            }
            if (from > to) std::swap(from, to);
            return true;
        };
        char f{'A'}, t{'A'};
        if (!parseOne(f,t)) throw expr::BasicError(2, "Missing letter list", pos);
        applyRange(f,t);
        while (true) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || !(
                b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")
            )) break;
            ++pos; // consume comma
            if (!parseOne(f,t)) throw expr::BasicError(2, "Missing letter after comma", pos);
            applyRange(f,t);
        }
    }

    // DEFINT/DEFSNG/DEFDBL/DEFSTR implementation
    uint16_t doDEFType(const std::vector<uint8_t>& b, size_t& pos, gwbasic::ScalarType type) {
        skipSpaces(b, pos);
        parseLetterRanges(b, pos, [&](char from, char to){ this->deftbl.setRange(from, to, type); });
        // Be defensive: consume any trailing whitespace/tokens up to end-of-statement
        // so nothing remains to be treated as an implied LET.
        skipSpaces(b, pos);
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            // Allow optional commas in some dialects after the last range; skip safely
            // Also skip any stray operator tokens that might have been produced by the tokenizer
            ++pos;
            skipSpaces(b, pos);
        }
        return 0;
    }

    // POKE addr, value (addr is offset in current DEF SEG)
    uint16_t doPOKE(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        auto aRes = ev.evaluate(b, pos, env);
        pos = aRes.nextPos;
        int32_t ofs = expr::ExpressionEvaluator::toInt16(aRes.value);
        skipSpaces(b, pos);
        if (atEnd(b, pos) || !(b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ","))) {
            throw expr::BasicError(2, "Expected comma in POKE", pos);
        }
        ++pos;
        skipSpaces(b, pos);
        auto vRes = ev.evaluate(b, pos, env);
        pos = vRes.nextPos;
        int32_t val = expr::ExpressionEvaluator::toInt16(vRes.value);
        if (ofs < 0 || ofs > 0xFFFF) throw expr::BasicError(5, "Illegal function call: address out of range", pos);
        uint32_t phys = static_cast<uint32_t>(this->currentSegment) * 16u + static_cast<uint32_t>(ofs);
        if (phys >= this->dosMemory.size()) throw expr::BasicError(5, "Illegal function call: address out of range", pos);
        this->dosMemory[phys] = static_cast<uint8_t>(val & 0xFF);
        return 0;
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
        // Expect TO keyword (tokenized or ASCII)
        bool hasTo = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80) {
                std::string n = tok ? tok->getTokenName(t) : std::string{};
                if (n == "TO") { ++pos; hasTo = true; }
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
        debugDumpTokens("doON:entry", b, pos);
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
                
                // Set or disable error trap
                if (handlerLine == 0) {
                    // ON ERROR GOTO 0 disables error handling
                    eventTraps.setErrorTrap(0);
                    runtimeStack.disableErrorHandler();
                } else {
                    // Set error trap to specified line
                    eventTraps.setErrorTrap(handlerLine);
                    runtimeStack.setErrorHandler(handlerLine);
                }
                
                return 0;
                
            } else if (eventKeyword == "KEY") {
                // ON KEY(n) GOTO line
                if (t >= 0x80) ++pos; // consume token
                skipSpaces(b, pos);
                
                // Parse KEY(n) or just KEY
                uint8_t keyIndex = 0;
                if (!atEnd(b, pos) && isSymbolAt(b, pos, '(', "(")) {
                    // KEY(n) syntax
                    ++pos; // consume '('
                    
                    skipSpaces(b, pos);
                    auto res = ev.evaluate(b, pos, env);
                    pos = res.nextPos;
                    keyIndex = static_cast<uint8_t>(expr::ExpressionEvaluator::toInt16(res.value));
                    
                    skipSpaces(b, pos);
                    if (!atEnd(b, pos) && isSymbolAt(b, pos, ')', ")")) {
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
                if (!atEnd(b, pos) && isSymbolAt(b, pos, '(', "(")) {
                    // TIMER(interval) syntax
                    ++pos; // consume '('
                    
                    skipSpaces(b, pos);
                    auto res = ev.evaluate(b, pos, env);
                    pos = res.nextPos;
                    interval = static_cast<uint16_t>(expr::ExpressionEvaluator::toInt16(res.value));
                    
                    skipSpaces(b, pos);
                    if (!atEnd(b, pos) && isSymbolAt(b, pos, ')', ")")) {
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
    debugDumpTokens("doON:before-index-eval", b, pos);
    auto res = ev.evaluate(b, pos, env);
        pos = res.nextPos;
    debugDumpTokens("doON:after-index-eval", b, pos);
        
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
                    debugDumpTokens("doON:unexpected-keyword", b, pos);
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
                    debugDumpTokens("doON:ascii-keyword-fail", b, pos);
                    throw expr::BasicError(2, "Expected GOTO or GOSUB in ON statement", pos);
                }
            }
        } else {
            debugDumpTokens("doON:missing-keyword", b, pos);
            throw expr::BasicError(2, "Expected GOTO or GOSUB in ON statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse line number list - look for sequence of integer tokens
        std::vector<uint16_t> lineNumbers;
        
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) break;
            
            // Check for comma separator first (ASCII or tokenized)
            if (isSymbolAt(b, pos, ',', ",")) { ++pos; continue; }
            
            // Try to parse line number using readLineNumber which understands tokenized integers
            try {
                debugDumpTokens("doON:before-readLineNumber", b, pos);
                uint16_t lineNum = readLineNumber(b, pos);
                lineNumbers.push_back(lineNum);
            } catch (...) {
                debugDumpTokens("doON:readLineNumber-exception", b, pos);
                // Not a valid line number - end of list
                break;
            }
            
            skipSpaces(b, pos);
            
            // Optional comma - if present, consume it for next iteration
            if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) { ++pos; }
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
            } else if (isSymbolAt(b, pos, '(', "(")) {  // tokenized left parenthesis
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
                if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) { // tokenized or ASCII comma
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
                if (!atEnd(b, pos) && isSymbolAt(b, pos, ')', ")")) { // tokenized right parenthesis
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
            if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) { // tokenized or ASCII comma
                ++pos; // consume comma
                continue;
            }
            
            // End of DIM statement
            break;
        }
        
        return 0;
    }
    
    // DEF FN statement: DEF FNname[(param1[,param2...])] = expression
    uint16_t doDEF(const std::vector<uint8_t>& b, size_t& pos) {
        auto logDebug = [&](const char* tag){
            try {
                std::ofstream ofs("/tmp/gwbasic_def_debug.log", std::ios::app);
                if (ofs) {
                    ofs << tag << " pos=" << pos << " bytes:";
                    size_t dbgEnd = std::min(b.size(), pos + 16);
                    for (size_t i = pos; i < dbgEnd; ++i) {
                        uint8_t c = b[i];
                        ofs << ' ' << std::hex << std::showbase << static_cast<int>(c) << std::dec;
                    }
                    ofs << " | tokens:";
                    for (size_t i = pos; i < dbgEnd; ++i) {
                        uint8_t c = b[i];
                        if (c >= 0x80 && tok) ofs << " [" << tok->getTokenName(c) << "]";
                        else if (c >= 32 && c < 127) ofs << ' ' << static_cast<char>(c);
                        else ofs << " (" << std::hex << std::showbase << static_cast<int>(c) << std::dec << ")";
                    }
                    ofs << '\n';
                }
            } catch (...) {}
        };

        skipSpaces(b, pos);
        logDebug("doDEF:after-skip");

        // First, handle DEF SEG [= expr] form
        {
            auto save = pos;
            std::string word;
            // Try tokenized first
            if (!atEnd(b, pos) && b[pos] >= 0x80 && tok) {
                std::string tname = tok->getTokenName(b[pos]);
                if (tname == "SEG") {
                    ++pos;
                    word = "SEG";
                }
            }
            // Or ASCII SEG
            if (word.empty()) {
                auto save2 = pos;
                while (!atEnd(b, pos) && std::isalpha(b[pos])) {
                    word.push_back(static_cast<char>(std::toupper(b[pos++])));
                    if (word.size() > 3) break; // only need to match SEG
                }
                if (word != "SEG") { pos = save2; word.clear(); }
            }
            if (word == "SEG") {
                skipSpaces(b, pos);
                bool hasEq = false;
                if (!atEnd(b, pos) && (
                    b[pos] == '=' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "=")
                )) { hasEq = true; ++pos; }
                if (hasEq) {
                    skipSpaces(b, pos);
                    auto segRes = ev.evaluate(b, pos, env);
                    pos = segRes.nextPos;
                    int32_t segVal = expr::ExpressionEvaluator::toInt16(segRes.value);
                    this->currentSegment = static_cast<uint16_t>(segVal & 0xFFFF);
                } else {
                    // DEF SEG with no argument resets to default (segment 0 in our emulation)
                    this->currentSegment = 0;
                }
                return 0;
            } else {
                pos = save; // not SEG; continue with DEF FN parsing
            }
        }
        
        // Expect FN followed by function name
        std::string fnKeyword;
        if (!atEnd(b, pos)) {
            // Prefer tokenizer-aware detection of the FN keyword
            if (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "FN") {
                fnKeyword = "FN";
                ++pos; // consume tokenized FN
                skipSpaces(b, pos); // allow optional space after FN
                logDebug("doDEF:consumed-token-FN");
            } else if (b[pos] >= 0x20 && b[pos] < 0x80) {
                // ASCII - try to read "FN" directly from source
                auto save = pos;
                for (int i = 0; i < 2 && !atEnd(b, pos) && b[pos] >= 0x20 && b[pos] < 0x80; ++i) {
                    fnKeyword.push_back(static_cast<char>(std::toupper(b[pos++])));
                }
                if (fnKeyword != "FN") {
                    pos = save;
                    fnKeyword.clear();
                } else {
                    skipSpaces(b, pos); // allow optional space after FN
                    logDebug("doDEF:consumed-ascii-FN");
                }
            }
        }
        
    // Read function name - should start immediately after FN or contain FN
    std::string funcName = readIdentifier(b, pos);
    if (funcName.empty()) {
            // Debug: dump next few bytes and token names to diagnose
            std::ostringstream oss;
            oss << "DEF debug near pos " << pos << ": ";
            size_t dbgEnd = std::min(b.size(), pos + 8);
            for (size_t i = pos; i < dbgEnd; ++i) {
                uint8_t c = b[i];
                if (c >= 0x80 && tok) {
                    oss << "[" << tok->getTokenName(c) << "] ";
                } else if (c >= 32 && c < 127) {
                    oss << "'" << static_cast<char>(c) << "' ";
                } else {
                    oss << std::hex << std::showbase << static_cast<int>(c) << std::dec << ' ';
                }
            }
            reportError(oss.str());
            throwBasicError(2, "Expected function name after DEF", pos);
        }
        
        // If we didn't see a separate FN keyword, the function name should start with FN
        if (fnKeyword.empty()) {
            if (funcName.length() < 2 || funcName.substr(0, 2) != "FN") {
                throwBasicError(2, "Function name must start with FN", pos);
            }
        }
        // If we had separate FN keyword, the function name is used as-is (don't prepend FN)
        
        skipSpaces(b, pos);
        
    // Parse optional parameter list
    std::vector<std::string> parameters;
    if (!atEnd(b, pos) && (
        b[pos] == '('
         || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "(")
        )) {
        // Consume opening parenthesis (ASCII or tokenized)
        ++pos;
            
            skipSpaces(b, pos);
            
            // Parse parameter list
        while (!atEnd(b, pos) && !(
            b[pos] == ')'
         || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ")")
        )) {
                skipSpaces(b, pos);
                if (atEnd(b, pos)) {
                    throwBasicError(2, "Missing closing parenthesis in DEF", pos);
                }
                
                std::string param = readIdentifier(b, pos);
                if (param.empty()) {
                    throwBasicError(2, "Expected parameter name in DEF", pos);
                }
                
                parameters.push_back(param);
                
                skipSpaces(b, pos);
                
        // Check for comma (more parameters)
        if (!atEnd(b, pos) && (
            b[pos] == ','
             || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")
            )) {
            ++pos; // consume comma
                    continue;
                }
                
                // Should be at closing parenthesis
                break;
            }
            
        // Expect closing parenthesis
        if (!atEnd(b, pos) && (
            b[pos] == ')'
         || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ")")
        )) {
        ++pos; // consume closing parenthesis
            } else {
                throwBasicError(2, "Missing closing parenthesis in DEF", pos);
            }
        }
        
        skipSpaces(b, pos);
        
        // Expect equals sign
        if (atEnd(b, pos) || !(
            b[pos] == '=' ||
            (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "=")
        )) {
            throwBasicError(2, "Expected = in DEF statement", pos);
        }
        ++pos; // consume '='
        
        skipSpaces(b, pos);
        
        // Capture the expression tokens for later evaluation
        size_t exprStart = pos;
        std::vector<uint8_t> expression;
        
        // Read tokens until end of statement or line terminator
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            expression.push_back(b[pos]);
            ++pos;
        }
        
        // Add null terminator to expression
        expression.push_back(0x00);
        
        // Determine return type from function name
        gwbasic::ScalarType returnType = gwbasic::ScalarType::Single; // Default
        if (!funcName.empty()) {
            char lastChar = funcName.back();
            switch (lastChar) {
                case '%': returnType = gwbasic::ScalarType::Int16; break;
                case '#': returnType = gwbasic::ScalarType::Double; break;
                case '$': returnType = gwbasic::ScalarType::String; break;
                case '!': returnType = gwbasic::ScalarType::Single; break;
                default:  returnType = gwbasic::ScalarType::Single; break;
            }
        }
        
        // Define the function
        if (!userFunctionManager.defineFunction(funcName, parameters, expression, returnType)) {
            throwBasicError(10, "Duplicate function definition: " + funcName, pos);
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
            uint16_t handlerLine = runtimeStack.getErrorHandlerLine();
            
            // Set up error frame - update existing frame instead of pushing new one
            auto* errFrame = runtimeStack.topErr();
            if (errFrame) {
                errFrame->errCode = errorCode;
                errFrame->resumeLine = currentLine;
                errFrame->resumeTextPtr = 0;
            } else {
                // No existing frame, create a new one
                gwbasic::ErrFrame frame{};
                frame.errCode = errorCode;
                frame.resumeLine = currentLine;
                frame.resumeTextPtr = 0;
                frame.errorHandlerLine = handlerLine;
                frame.trapEnabled = true;
                runtimeStack.pushErr(frame);
            }
            
            // Jump to error handler
            return handlerLine;
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

    uint16_t doSCREEN(const std::vector<uint8_t>& b, size_t& pos) {
        // SCREEN statement implementation
        // Syntax: SCREEN mode[,colorswitch][,apage][,vpage]
        
        skipSpaces(b, pos);
        
        // Parse parameters - SCREEN uses integer parameters
        std::vector<int16_t> params;
        
        // Parse mode parameter (required)
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            throwBasicError(5, "Missing mode parameter in SCREEN", pos);
        }
        
        auto res = ev.evaluate(b, pos, env);
        pos = res.nextPos;
        int16_t mode = expr::ExpressionEvaluator::toInt16(res.value);
        params.push_back(mode);
        
        skipSpaces(b, pos);
        
        // Parse optional parameters (colorswitch, apage, vpage)
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            // Check for comma
            if (b[pos] == ',' || b[pos] == 0xF5) { // 0xF5 is tokenized comma
                ++pos; // consume comma
                skipSpaces(b, pos);
                
                // Check for null parameter (two commas in a row)
                if (!atEnd(b, pos) && (b[pos] == ',' || b[pos] == 0xF5 || b[pos] == ':' || b[pos] == 0x00)) {
                    params.push_back(0); // null parameter defaults to 0
                    continue;
                }
                
                // Parse parameter
                auto res = ev.evaluate(b, pos, env);
                pos = res.nextPos;
                params.push_back(expr::ExpressionEvaluator::toInt16(res.value));
                
                skipSpaces(b, pos);
            } else {
                break; // End of parameter list
            }
        }

        // Defensively consume any trailing tokens up to end-of-statement so
        // nothing remains to be misinterpreted as a new statement in
        // immediate mode. This mirrors COLOR/WIDTH cleanup logic.
        skipSpaces(b, pos);
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            uint8_t t = b[pos];
            if (t == '"') {
                // Skip string literal
                ++pos;
                while (!atEnd(b, pos) && b[pos] != '"' && b[pos] != 0x00) ++pos;
                if (!atEnd(b, pos) && b[pos] == '"') ++pos;
            } else if (t == 0x11) {
                // Integer constant token (0x11 LL HH)
                pos += 3;
            } else if (t == 0x1D) {
                // Single precision float (0x1D + 4 bytes)
                pos += 5;
            } else if (t == 0x1F) {
                // Double precision float (0x1F + 8 bytes)
                pos += 9;
            } else {
                // Any other token/operator/identifier; consume one byte
                ++pos;
            }
            skipSpaces(b, pos);
        }
        
        // Execute SCREEN command based on mode
        return executeScreenMode(mode, params);
    }
    
    uint16_t executeScreenMode(int16_t mode, const std::vector<int16_t>& params) {
        // Validate mode range
        if (mode < 0 || mode > 13 || (mode >= 3 && mode <= 6)) {
            throwBasicError(5, "Illegal function call: Invalid screen mode " + std::to_string(mode), 0);
            return 0;
        }
        
        // Try to change screen mode via callback
        bool success = false;
        if (screenModeCallback) {
            success = screenModeCallback(mode);
        }
        
        if (!success && screenModeCallback) {
            throwBasicError(5, "Illegal function call: Could not set screen mode " + std::to_string(mode), 0);
            return 0;
        }
        
        // Initialize graphics context for the new mode
        if (graphicsBufferCallback) {
            uint8_t* buffer = graphicsBufferCallback();
            if (buffer) {
                graphics.setMode(mode, buffer);
            }
        }

        // Remember current mode to avoid resetting to defaults later
        currentGraphicsMode_ = mode;
        
        // Report mode change success
        switch (mode) {
            case 0:
                // Text mode (80x25 or 40x25)
                if (printCallback) {
                    printCallback("SCREEN 0: Text mode 80x25 - Active\n");
                }
                break;
                
            case 1:
                // CGA Graphics mode 320x200, 4 colors
                if (printCallback) {
                    printCallback("SCREEN 1: Graphics mode 320x200, 4 colors - Active\n");
                }
                break;
                
            case 2:
                // CGA Graphics mode 640x200, 2 colors
                if (printCallback) {
                    printCallback("SCREEN 2: Graphics mode 640x200, 2 colors - Active\n");
                }
                break;
                
            case 7:
                // EGA Graphics mode 320x200, 16 colors
                if (printCallback) {
                    printCallback("SCREEN 7: EGA mode 320x200, 16 colors - Active\n");
                }
                break;
                
            case 8:
                // EGA Graphics mode 640x200, 16 colors
                if (printCallback) {
                    printCallback("SCREEN 8: EGA mode 640x200, 16 colors - Active\n");
                }
                break;
                
            case 9:
                // EGA Graphics mode 640x350, 16 colors
                if (printCallback) {
                    printCallback("SCREEN 9: EGA mode 640x350, 16 colors - Active\n");
                }
                break;
                
            case 10:
                // EGA Graphics mode 640x350, 4 colors
                if (printCallback) {
                    printCallback("SCREEN 10: EGA mode 640x350, 4 colors - Active\n");
                }
                break;
                
            case 11:
                // VGA Graphics mode 640x480, 2 colors
                if (printCallback) {
                    printCallback("SCREEN 11: VGA mode 640x480, 2 colors - Active\n");
                }
                break;
                
            case 12:
                // VGA Graphics mode 640x480, 16 colors
                if (printCallback) {
                    printCallback("SCREEN 12: VGA mode 640x480, 16 colors - Active\n");
                }
                break;
                
            case 13:
                // VGA Graphics mode 320x200, 256 colors
                if (printCallback) {
                    printCallback("SCREEN 13: VGA mode 320x200, 256 colors - Active\n");
                }
                break;
                
            default:
                throwBasicError(5, "Illegal function call: Invalid screen mode " + std::to_string(mode), 0);
                break;
        }
        
        return 0;
    }

    uint16_t doCOLOR(const std::vector<uint8_t>& b, size_t& pos) {
        // COLOR statement implementation
        // Syntax: COLOR [foreground][,[background][,border]]
        // In GW-BASIC: COLOR foreground,background,border
        // For simplicity, we'll implement: COLOR foreground[,background]
        
        skipSpaces(b, pos);
        
    int16_t foreground = -1;  // -1 means no change
    int16_t background = -1;  // -1 means no change
    int16_t /*border*/ border = -1; // parsed but ignored
        
        // Parse foreground color (optional)
        if (!atEnd(b, pos) && !(
                b[pos] == ',' ||
                (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",") ||
                b[pos] == ':' || b[pos] == 0x00)) {
            auto res = ev.evaluate(b, pos, env);
            pos = res.nextPos;
            foreground = expr::ExpressionEvaluator::toInt16(res.value);
            
            // Validate foreground color range (0-15 for CGA/EGA/VGA)
            if (foreground < 0 || foreground > 15) {
                throwBasicError(5, "Illegal function call: Invalid foreground color " + std::to_string(foreground), pos);
                return 0;
            }
        }
        
        skipSpaces(b, pos);
        
        // Check for comma (background parameter)
        if (!atEnd(b, pos) && (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ","))) {
            ++pos; // consume comma
            skipSpaces(b, pos);
            
            // Parse background color (optional)
            if (!atEnd(b, pos) && !(b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")) && b[pos] != ':' && b[pos] != 0x00) {
                auto res = ev.evaluate(b, pos, env);
                pos = res.nextPos;
                background = expr::ExpressionEvaluator::toInt16(res.value);
                
                // Validate background color range (0-7 for most modes)
                if (background < 0 || background > 7) {
                    throwBasicError(5, "Illegal function call: Invalid background color " + std::to_string(background), pos);
                    return 0;
                }
            }

            // Optional border parameter (ignored). Consume if present to avoid trailing tokens.
            skipSpaces(b, pos);
            if (!atEnd(b, pos) && (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ","))) {
                ++pos; // consume comma before border
                skipSpaces(b, pos);
                if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                    auto res = ev.evaluate(b, pos, env);
                    pos = res.nextPos;
                    border = expr::ExpressionEvaluator::toInt16(res.value);
                    // Validate border range defensively (0-15)
                    if (border < 0 || border > 15) {
                        throwBasicError(5, "Illegal function call: Invalid border color " + std::to_string(border), pos);
                        return 0;
                    }
                }
            }
        }
        
        // Eat trailing tokens defensively up to end-of-statement. This prevents
        // any leftover separators or tokens from being misinterpreted as new
        // statements in immediate mode.
        skipSpaces(b, pos);
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            uint8_t t = b[pos];
            if (t == '"') {
                // Skip string literal
                ++pos;
                while (!atEnd(b, pos) && b[pos] != '"' && b[pos] != 0x00) ++pos;
                if (!atEnd(b, pos) && b[pos] == '"') ++pos;
            } else if (t == 0x11) {
                // Integer constant token
                pos += 3;
            } else if (t == 0x1D) {
                // Single precision float token
                pos += 5;
            } else if (t == 0x1F) {
                // Double precision float token
                pos += 9;
            } else {
                // Any other token/operator/identifier; consume one byte
                ++pos;
            }
            skipSpaces(b, pos);
        }

        // Execute COLOR command
        return executeColorChange(foreground, background);
    }
    
    uint16_t executeColorChange(int16_t foreground, int16_t background) {
        // Try to change colors via callback
        bool success = false;
        if (colorCallback) {
            success = colorCallback(foreground, background);
        }
        
        if (!success && colorCallback) {
            throwBasicError(5, "Illegal function call: Could not set color", 0);
            return 0;
        }
        
        // Update graphics context default color
        if (foreground >= 0) {
            graphics.setDefaultColor(static_cast<uint8_t>(foreground));
        }
        
        // Report color change
        if (printCallback) {
            std::string message = "COLOR";
            if (foreground >= 0) {
                message += " " + std::to_string(foreground);
            }
            if (background >= 0) {
                message += "," + std::to_string(background);
            }
            message += " - Active\n";
            printCallback(message);
        }
        
        return 0;
    }

    // WIDTH columns
    uint16_t doWIDTH(const std::vector<uint8_t>& b, size_t& pos) {
        // Syntax: WIDTH n   (supported: 40, 80, 132). Other WIDTH forms are not yet implemented.
        skipSpaces(b, pos);
        if (atEnd(b, pos) || b[pos] == ':' || b[pos] == 0x00) {
            throwBasicError(5, "Missing width parameter", pos);
        }

        auto res = ev.evaluate(b, pos, env);
        pos = res.nextPos;
        int16_t cols = expr::ExpressionEvaluator::toInt16(res.value);

        // Validate supported widths
        if (!(cols == 40 || cols == 80 || cols == 132)) {
            throwBasicError(5, "Illegal function call: Invalid width " + std::to_string(cols), pos);
            return 0;
        }

        // Consume trailing tokens to end-of-statement defensively
        skipSpaces(b, pos);
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            // Skip any stray tokens
            ++pos;
            skipSpaces(b, pos);
        }

        return executeWidthChange(cols);
    }

    uint16_t executeWidthChange(int16_t cols) {
        bool success = true;
        if (widthCallback) {
            success = widthCallback(cols);
        }
        if (!success) {
            throwBasicError(5, "Illegal function call: Could not set width", 0);
            return 0;
        }

        if (printCallback) {
            std::string msg = "WIDTH " + std::to_string(cols) + " - Active\n";
            printCallback(msg);
        }
        return 0;
    }

    // LOCATE [row][,[column][,[cursor][,[start][,[stop]]]]]
    // Parse parameters and, if a locateCallback is provided by the host, request cursor/state update.
    // Null parameters between commas are allowed (e.g., LOCATE ,10). -1 indicates "no change".
    uint16_t doLOCATE(const std::vector<uint8_t>& b, size_t& pos) {
        auto isTokenNamed = [&](size_t p, const char* name)->bool {
            if (p >= b.size()) return false;
            uint8_t t = b[p];
            if (t < 0x80) {
                // ASCII single-char match (e.g., ',', ':')
                return name && name[1] == '\0' && static_cast<uint8_t>(name[0]) == t;
            }
            return tok && tok->getTokenName(t) == name;
        };
        auto isComma = [&](size_t p)->bool { return isTokenNamed(p, ","); };
        auto isColon = [&](size_t p)->bool { return isTokenNamed(p, ":"); };
        auto isEnd   = [&](size_t p)->bool { return p >= b.size() || b[p] == 0x00 || isColon(p); };

        skipSpaces(b, pos);

        auto tryParseInt = [&](int16_t& out)->bool {
            if (isEnd(pos) || isComma(pos)) return false; // null parameter
            auto res = ev.evaluate(b, pos, env);
            pos = res.nextPos;
            out = expr::ExpressionEvaluator::toInt16(res.value);
            return true;
        };

    int16_t row = -1, col = -1, cursor = -1, start = -1, stop = -1;
        (void)row; (void)col; (void)cursor; (void)start; (void)stop;

        // row
        (void)tryParseInt(row);
        skipSpaces(b, pos);
        if (isComma(pos)) {
            ++pos; skipSpaces(b, pos);
            // column
            (void)tryParseInt(col);
            skipSpaces(b, pos);
            if (isComma(pos)) {
                ++pos; skipSpaces(b, pos);
                // cursor
                (void)tryParseInt(cursor);
                skipSpaces(b, pos);
                if (isComma(pos)) {
                    ++pos; skipSpaces(b, pos);
                    // start
                    (void)tryParseInt(start);
                    skipSpaces(b, pos);
                    if (isComma(pos)) {
                        ++pos; skipSpaces(b, pos);
                        // stop
                        (void)tryParseInt(stop);
                        skipSpaces(b, pos);
                    }
                }
            }
        }

        // If host provided a callback, request the cursor/state change now
        if (locateCallback) {
            // Convert to host-consumed ints; BASIC rows/cols are 1-based; -1 means no change
            // We do not enforce bounds here; host can clamp to its screen size.
            bool ok = false;
            try {
                ok = locateCallback(static_cast<int>(row), static_cast<int>(col), static_cast<int>(cursor), static_cast<int>(start), static_cast<int>(stop));
            } catch (const std::exception&) {
                ok = false;
            }
            if (!ok) {
                // Follow GW-BASIC behavior: invalid coordinates -> Illegal function call
                // but keep tolerant for partial implementations: only throw if explicit bad inputs
                // e.g., row or col equals 0 (GW-BASIC rows/cols start at 1 when specified)
                if ((row == 0) || (col == 0)) {
                    throwBasicError(5, "Illegal function call: LOCATE", pos);
                }
            }
        }

        // Defensively consume any trailing tokens to EOL or ':' so nothing remains for implied LET
        skipSpaces(b, pos);
        while (!isEnd(pos)) {
            uint8_t t = b[pos];
            if (t == '"') {
                ++pos; while (!atEnd(b, pos) && b[pos] != '"' && b[pos] != 0x00) ++pos; if (!atEnd(b, pos) && b[pos] == '"') ++pos;
            } else if (t == 0x11) { // integer literal token
                pos += 3;
            } else if (t == 0x1D) { // single
                pos += 5;
            } else if (t == 0x1F) { // double
                pos += 9;
            } else {
                ++pos;
            }
            skipSpaces(b, pos);
        }
        return 0;
    }

    // CLS [n]
    // Clear the display; optional numeric argument is parsed and ignored for now.
    uint16_t doCLS(const std::vector<uint8_t>& b, size_t& pos) {
        skipSpaces(b, pos);
        // Optional argument (zone) is allowed in GW-BASIC; we accept and ignore.
        if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            auto savePos = pos;
            try {
                auto res = ev.evaluate(b, pos, env);
                (void)res; // ignore value
            } catch (...) {
                // Not an expression, revert
                pos = savePos;
            }
        }

        // Consume trailing to end-of-statement
        skipSpaces(b, pos);
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            uint8_t t = b[pos];
            if (t == '"') {
                ++pos; while (!atEnd(b, pos) && b[pos] != '"' && b[pos] != 0x00) ++pos; if (!atEnd(b, pos) && b[pos] == '"') ++pos;
            } else if (t == 0x11) { pos += 3; }
            else if (t == 0x1D) { pos += 5; }
            else if (t == 0x1F) { pos += 9; }
            else { ++pos; }
            skipSpaces(b, pos);
        }

        bool ok = true;
        if (clsCallback) {
            try { ok = clsCallback(); } catch (...) { ok = false; }
        }
        if (!ok) {
            throwBasicError(5, "Illegal function call: CLS", pos);
        }
        return 0;
    }

    uint16_t doINPUT(const std::vector<uint8_t>& b, size_t& pos) {
        // INPUT statement implementation
        // Syntax: INPUT [prompt$;] variable[,variable]...
        // or: INPUT# filenumber, variable[,variable]...
        
        skipSpaces(b, pos);
        
        // Check for INPUT# (file input) syntax first
        uint8_t fileNumber = 0; // 0 means console input
        bool isFileInput = false;
        
    // Check if starts with # (file number) - ASCII or tokenized
    if (!atEnd(b, pos) && (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            ++pos; // consume #
            skipSpaces(b, pos);
            
            // Parse file number
            auto fileNumRes = ev.evaluate(b, pos, env);
            pos = fileNumRes.nextPos;
            
            fileNumber = static_cast<uint8_t>(expr::ExpressionEvaluator::toInt16(fileNumRes.value));
            isFileInput = true;
            
            // Validate file number and check if open for input
            if (!gwbasic::FileManager::isValidFileNumber(fileNumber)) {
                throwBasicError(52, "Bad file number", pos);
            }
            
            if (!fileManager.isFileOpen(fileNumber)) {
                throwBasicError(62, "Input past end", pos);
            }
            
            auto* fileHandle = fileManager.getFile(fileNumber);
            if (!fileHandle || fileHandle->mode != gwbasic::FileMode::INPUT) {
                throwBasicError(54, "Bad file mode", pos);
            }
            
            skipSpaces(b, pos);
            
            // Expect comma after file number (ASCII or tokenized)
            if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) {
                ++pos; // consume comma
                skipSpaces(b, pos);
            } else {
                throwBasicError(2, "Expected comma after file number in INPUT#", pos);
            }
            
            // For file input, skip prompt parsing - jump to variable parsing
            return doFileInput(b, pos, fileNumber);
        }
        
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
                    
                    // Check for separator after prompt (semicolon or comma, ASCII or tokenized)
                    if (!atEnd(b, pos) && (isSymbolAt(b, pos, ';', ";") || isSymbolAt(b, pos, ',', ","))) {
                        // Check separator type
                        if (isSymbolAt(b, pos, ';', ";")) {
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
                    if (!atEnd(b, pos) && (isSymbolAt(b, pos, ';', ";") || isSymbolAt(b, pos, ',', ","))) {
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
                        if (isSymbolAt(b, pos, ';', ";")) {
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
        
        // Display prompt if present (use callback only; console/GUI wires it appropriately)
        if (hasPrompt) {
            if (printCallback) {
                printCallback(prompt);
                if (!suppressNewline) {
                    printCallback(" ");
                }
            }
        } else {
            // Default prompt
            if (printCallback) {
                printCallback("? ");
            }
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
            if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) {
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

    // Helper method for INPUT# file input
    uint16_t doFileInput(const std::vector<uint8_t>& b, size_t& pos, uint8_t fileNumber) {
        // Get list of variables to read into
        std::vector<std::string> variables;
        
        while (!atEnd(b, pos)) {
            skipSpaces(b, pos);
            if (atEnd(b, pos) || b[pos] == ':') break;
            
            auto varName = readIdentifier(b, pos);
            if (varName.empty()) {
                throw expr::BasicError(2, "Expected variable name in INPUT#", pos);
            }
            
            variables.push_back(varName);
            
            skipSpaces(b, pos);
            
            // Check for comma (more variables)
            if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) {
                ++pos; // consume comma
                continue;
            }
            
            // End of variable list
            break;
        }
        
        if (variables.empty()) {
            throw expr::BasicError(2, "No variables specified in INPUT#", pos);
        }
        
        // Read from file and assign to variables
        std::string inputLine;
        if (!fileManager.readLine(fileNumber, inputLine)) {
            // Check if EOF
            if (fileManager.isEOF(fileNumber)) {
                throwBasicError(62, "Input past end", pos);
            } else {
                throwBasicError(57, "Device I/O error", pos);
            }
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
            gwbasic::Value value;
            if (!dataManager.readValue(value)) {
                throwBasicError(4, "Out of DATA", pos);
            }
            
            // Assign the value to the variable
            assignDataValue(varName, value);
        }
        
        // Skip any trailing spaces or null bytes
        while (pos < b.size() && (isSpace(b[pos]) || b[pos] == 0x00)) {
            ++pos;
        }
        
        return 0;
    }
    
    // DATA statement implementation
    uint16_t doDATA(const std::vector<uint8_t>& b, size_t& pos) {
        // DATA statements are passive - they just contain data to be read
        // The DataManager handles parsing them when READ statements are executed
        // But we need to skip over the data values to avoid parsing them as statements
        
        // Skip over all data values until we reach end of line or statement separator
        while (!atEnd(b, pos) && !(b[pos] == ':' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ":"))) {
            // Skip whitespace
            while (!atEnd(b, pos) && (b[pos] == ' ' || b[pos] == '\t')) {
                pos++;
            }
            
            if (atEnd(b, pos) || b[pos] == ':' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ":")) break;
            
            // Skip comma separator
            if (b[pos] == ',' || b[pos] == 0xF5 || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")) {
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
            } else if (token == 0x1D) {
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
    
    // REM statement implementation
    uint16_t doREM(const std::vector<uint8_t>& b, size_t& pos) {
        // REM statement - skip everything to end of line or statement separator
        // Everything after REM is a comment and should be ignored
        
        while (!atEnd(b, pos) && b[pos] != 0x00 && b[pos] != ':' && 
               !(b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ":")) {
            ++pos;
        }
        
        return 0;
    }

private:
    // Helper method to assign a data value to a variable
    void assignDataValue(const std::string& varName, const gwbasic::Value& value) {
        // Get or create variable
        gwbasic::VarSlot& slot = vars.getOrCreate(varName);
        
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
    }

    // Graphics statement handlers
    uint16_t doPSET(const std::vector<uint8_t>& b, size_t& pos) {
        // PSET [STEP] (x,y) [,color]
        skipSpaces(b, pos);
        debugDumpTokens("PSET:entry", b, pos);
        
        // Check for optional STEP keyword
        bool stepMode = false;
        if (consumeKeyword(b, pos, "STEP")) { stepMode = true; skipSpaces(b, pos); debugDumpTokens("PSET:after-STEP", b, pos); }
        
        // Expect opening parenthesis (ASCII or tokenized)
        if (!consumeSymbol(b, pos, '(', "(")) {
            debugDumpTokens("PSET:expected-lparen", b, pos);
            throwBasicError(2, "Expected ( in PSET statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse x coordinate
        auto xRes = ev.evaluate(b, pos, env);
        pos = xRes.nextPos;
        int x = expr::ExpressionEvaluator::toInt16(xRes.value);
        
        skipSpaces(b, pos);
        
        int y = 0; // declare y variable
        
        // Expect comma between x and y
        if (consumeSymbol(b, pos, ',', ",")) {
            skipSpaces(b, pos);
            
            // Parse y coordinate
            auto yRes = ev.evaluate(b, pos, env);
            pos = yRes.nextPos;
            y = expr::ExpressionEvaluator::toInt16(yRes.value);
        } else {
            throwBasicError(2, "Expected comma between coordinates in PSET", pos);
        }
        
        skipSpaces(b, pos);
        
        // Expect closing parenthesis (ASCII or tokenized)
        if (!consumeSymbol(b, pos, ')', ")")) {
            debugDumpTokens("PSET:expected-rparen", b, pos);
            throwBasicError(2, "Expected ) in PSET statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Optional color parameter
        int color = -1; // -1 means use default
        if (consumeSymbol(b, pos, ',', ",")) {
            skipSpaces(b, pos);
            
            if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                auto colorRes = ev.evaluate(b, pos, env);
                pos = colorRes.nextPos;
                color = expr::ExpressionEvaluator::toInt16(colorRes.value);
                
                if (color < 0 || color > 15) {
                    throwBasicError(5, "Illegal function call: Invalid color", pos);
                }
            }
        }
        
        // Initialize graphics context if needed
        initializeGraphicsContext();
        
        // Execute PSET
        if (!graphics.pset(x, y, color, stepMode)) {
            // PSET failure is typically not an error in GW-BASIC
        }
        
        return 0;
    }
    
    uint16_t doPRESET(const std::vector<uint8_t>& b, size_t& pos) {
        // PRESET is PSET with color 0 (background)
        // Parse like PSET but force color to 0
        skipSpaces(b, pos);
        debugDumpTokens("PRESET:entry", b, pos);
        
        // Check for optional STEP keyword
        bool stepMode = false;
        if (consumeKeyword(b, pos, "STEP")) { stepMode = true; skipSpaces(b, pos); debugDumpTokens("PRESET:after-STEP", b, pos); }
        
        // Expect opening parenthesis (ASCII or tokenized)
        if (!consumeSymbol(b, pos, '(', "(")) {
            throwBasicError(2, "Expected ( in PRESET statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse x coordinate
        auto xRes = ev.evaluate(b, pos, env);
        pos = xRes.nextPos;
        int x = expr::ExpressionEvaluator::toInt16(xRes.value);
        
        skipSpaces(b, pos);
        
        // Expect comma (ASCII or tokenized)
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma in PRESET coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse y coordinate
        auto yRes = ev.evaluate(b, pos, env);
        pos = yRes.nextPos;
        int y = expr::ExpressionEvaluator::toInt16(yRes.value);
        
        skipSpaces(b, pos);
        
        // Expect closing parenthesis (ASCII or tokenized)
        if (!consumeSymbol(b, pos, ')', ")")) {
            throwBasicError(2, "Expected ) in PRESET statement", pos);
        }
        
        // Initialize graphics context if needed
        initializeGraphicsContext();
        
        // Execute PRESET (always color 0)
        if (!graphics.pset(x, y, 0, stepMode)) {
            // PRESET failure is typically not an error in GW-BASIC
        }
        
        return 0;
    }
    
    uint16_t doLINE(const std::vector<uint8_t>& b, size_t& pos) {
        // LINE [STEP] [(x1,y1)]-(x2,y2) [,color] [,[B][F]]
        skipSpaces(b, pos);
        
        // Check for optional STEP keyword
        bool stepMode = false;
        if (consumeKeyword(b, pos, "STEP")) { stepMode = true; skipSpaces(b, pos); }
        
        bool hasStartPoint = true;
        int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
        
        // Check if starts with - (no start point)
        if (!atEnd(b, pos) && isSymbolAt(b, pos, '-', "-")) {
            hasStartPoint = false;
            ++pos; // consume -
        } else if (!atEnd(b, pos) && isSymbolAt(b, pos, '(', "(")) {
            // Parse start point
            ++pos; // consume (
            skipSpaces(b, pos);
            
            auto xRes = ev.evaluate(b, pos, env);
            pos = xRes.nextPos;
            x1 = expr::ExpressionEvaluator::toInt16(xRes.value);
            
            skipSpaces(b, pos);
            
            if (!consumeSymbol(b, pos, ',', ",")) {
                throwBasicError(2, "Expected comma in LINE start coordinates", pos);
            }
            
            skipSpaces(b, pos);
            
            auto yRes = ev.evaluate(b, pos, env);
            pos = yRes.nextPos;
            y1 = expr::ExpressionEvaluator::toInt16(yRes.value);
            
            skipSpaces(b, pos);
            
            if (!consumeSymbol(b, pos, ')', ")")) {
                throwBasicError(2, "Expected ) in LINE start coordinates", pos);
            }
            
            skipSpaces(b, pos);
            
            // Expect - after start point (regular or tokenized)
            if (!isSymbolAt(b, pos, '-', "-")) {
                throwBasicError(2, "Expected - after start point in LINE", pos);
            }
            ++pos; // consume -
        } else {
            throwBasicError(2, "Expected coordinates or - in LINE statement", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse end point
        if (atEnd(b, pos) || !isSymbolAt(b, pos, '(', "(")) {
            throwBasicError(2, "Expected ( for end coordinates in LINE", pos);
        }
        ++pos; // consume (
        
        skipSpaces(b, pos);
        
        auto x2Res = ev.evaluate(b, pos, env);
        pos = x2Res.nextPos;
        x2 = expr::ExpressionEvaluator::toInt16(x2Res.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma in LINE end coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        auto y2Res = ev.evaluate(b, pos, env);
        pos = y2Res.nextPos;
        y2 = expr::ExpressionEvaluator::toInt16(y2Res.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ')', ")")) {
            throwBasicError(2, "Expected ) in LINE end coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        // Optional color parameter
        int color = -1;
        if (!atEnd(b, pos) && consumeSymbol(b, pos, ',', ",")) {
            skipSpaces(b, pos);
            
            if (!atEnd(b, pos) && b[pos] != ',' && b[pos] != ':' && b[pos] != 0x00) {
                auto colorRes = ev.evaluate(b, pos, env);
                pos = colorRes.nextPos;
                color = expr::ExpressionEvaluator::toInt16(colorRes.value);
                
                if (color < 0 || color > 15) {
                    throwBasicError(5, "Illegal function call: Invalid color", pos);
                }
            }
            
            skipSpaces(b, pos);
        }
        
        // Optional box flags (B, BF)
        bool isBox = false;
        bool isFilled = false;
        if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) {
            ++pos; // consume comma
            skipSpaces(b, pos);
            
            // Parse B or BF flags
            if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                if (b[pos] == 'B' || b[pos] == 'b') {
                    isBox = true;
                    ++pos;
                    
                    // Check for F after B
                    if (!atEnd(b, pos) && (b[pos] == 'F' || b[pos] == 'f')) {
                        isFilled = true;
                        ++pos;
                    }
                }
            }
        }
        
        // Initialize graphics context if needed
        initializeGraphicsContext();
        
        // Execute LINE
        if (isBox) {
            // Draw rectangle
            if (!graphics.rectangle(x1, y1, x2, y2, color, isFilled, stepMode)) {
                // Rectangle failure is typically not an error
            }
        } else {
            // Draw line
            if (hasStartPoint) {
                if (!graphics.line(x1, y1, x2, y2, color, stepMode, stepMode)) {
                    // Line failure is typically not an error
                }
            } else {
                if (!graphics.lineToLastPoint(x2, y2, color, stepMode)) {
                    // Line failure is typically not an error
                }
            }
        }
        
        return 0;
    }
    
    uint16_t doCIRCLE(const std::vector<uint8_t>& b, size_t& pos) {
        // CIRCLE [STEP] (x,y), radius [,color]
        skipSpaces(b, pos);
        debugDumpTokens("CIRCLE:entry", b, pos);
        
        // Check for optional STEP keyword
        bool stepMode = false;
        if (consumeKeyword(b, pos, "STEP")) { stepMode = true; skipSpaces(b, pos); debugDumpTokens("CIRCLE:after-STEP", b, pos); }
        
        // Expect opening parenthesis
        if (atEnd(b, pos) || !isSymbolAt(b, pos, '(', "(")) {
            debugDumpTokens("CIRCLE:expected-lparen", b, pos);
            throwBasicError(2, "Expected ( in CIRCLE statement", pos);
        }
        ++pos; // consume (
        
        skipSpaces(b, pos);
        
        // Parse center x coordinate
        auto xRes = ev.evaluate(b, pos, env);
        pos = xRes.nextPos;
        int x = expr::ExpressionEvaluator::toInt16(xRes.value);
        
        skipSpaces(b, pos);
        
        // Expect comma
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma in CIRCLE center coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse center y coordinate
        auto yRes = ev.evaluate(b, pos, env);
        pos = yRes.nextPos;
        int y = expr::ExpressionEvaluator::toInt16(yRes.value);
        
        skipSpaces(b, pos);
        
        // Expect closing parenthesis
        if (!consumeSymbol(b, pos, ')', ")")) {
            throwBasicError(2, "Expected ) in CIRCLE center coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        // Expect comma before radius
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma before radius in CIRCLE", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse radius
        auto radiusRes = ev.evaluate(b, pos, env);
        pos = radiusRes.nextPos;
        int radius = expr::ExpressionEvaluator::toInt16(radiusRes.value);
        
        if (radius < 0) {
            throwBasicError(5, "Illegal function call: Negative radius", pos);
        }
        
        skipSpaces(b, pos);
        
        // Optional color parameter
        int color = -1;
        if (!atEnd(b, pos) && consumeSymbol(b, pos, ',', ",")) {
            skipSpaces(b, pos);
            
            if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                auto colorRes = ev.evaluate(b, pos, env);
                pos = colorRes.nextPos;
                color = expr::ExpressionEvaluator::toInt16(colorRes.value);
                
                if (color < 0 || color > 15) {
                    throwBasicError(5, "Illegal function call: Invalid color", pos);
                }
            }
        }
        
        // Initialize graphics context if needed
        initializeGraphicsContext();
        
        // Execute CIRCLE
        if (!graphics.circle(x, y, radius, color, stepMode)) {
            // Circle failure is typically not an error
        }
        
        return 0;
    }
    
    uint16_t doDRAW(const std::vector<uint8_t>& b, size_t& pos) {
        // DRAW string_expression
        debugDumpTokens("DRAW:entry", b, pos);
        skipSpaces(b, pos);
        
        // Parse string expression
        auto result = ev.evaluate(b, pos, env);
        pos = result.nextPos;
        
        // Convert to string
        std::string drawString;
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Str>) {
                drawString = x.v;
            } else {
                throwBasicError(13, "Type mismatch: DRAW requires string", pos);
            }
        }, result.value);
        
        // Initialize graphics context if needed
        initializeGraphicsContext();
        
        // Execute DRAW command string
        executeDRAWString(drawString);
        
        return 0;
    }
    
    uint16_t doPLAY(const std::vector<uint8_t>& b, size_t& pos) {
        // PLAY string_expression
        debugDumpTokens("PLAY:entry", b, pos);
        skipSpaces(b, pos);
        
        // Parse string expression
        auto result = ev.evaluate(b, pos, env);
        pos = result.nextPos;
        
        // Convert to string
        std::string musicString;
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Str>) {
                musicString = x.v;
            } else {
                // For numeric values, convert to string representation
                if constexpr (std::is_same_v<T, expr::Int16>) {
                    musicString = std::to_string(x.v);
                } else if constexpr (std::is_same_v<T, expr::Single>) {
                    musicString = std::to_string(x.v);
                } else if constexpr (std::is_same_v<T, expr::Double>) {
                    musicString = std::to_string(x.v);
                }
            }
        }, result.value);
        
        // Parse and execute the music string
        playMusicString(musicString);
        
        return 0;
    }
    
    uint16_t doGET(const std::vector<uint8_t>& b, size_t& pos) {
        debugDumpTokens("GET:entry", b, pos);
        skipSpaces(b, pos);
        
        // Check for file GET: GET #filenumber, [recordnumber]
        if (!atEnd(b, pos) && (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            return doFileGET(b, pos);
        }
        
        // Check if it looks like a graphics GET by looking for opening parenthesis
        // Graphics GET syntax: GET [STEP] (x1,y1)-(x2,y2), arrayvar
        size_t lookAhead = pos;
        skipSpaces(b, lookAhead);
        
        // Skip optional STEP keyword
        if (!atEnd(b, lookAhead)) {
            uint8_t t = b[lookAhead];
            if (t >= 0x80 && tok && tok->getTokenName(t) == "STEP") {
                ++lookAhead;
                skipSpaces(b, lookAhead);
            }
        }
        
        // If we find an opening parenthesis, assume graphics GET
        if (!atEnd(b, lookAhead) && (b[lookAhead] == '(' || (b[lookAhead] >= 0x80 && tok && tok->getTokenName(b[lookAhead]) == "("))) {
            debugDumpTokens("GET:route-graphics", b, pos);
            return doGraphicsGET(b, pos);
        }
        
        // Default to file GET if syntax is ambiguous
        return doFileGET(b, pos);
    }
    
    uint16_t doFileGET(const std::vector<uint8_t>& b, size_t& pos) {
        // GET #filenumber, [recordnumber]
        
        // Consume the '#' token (ASCII or tokenized)
        if (!atEnd(b, pos) && (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            ++pos;
        }
        
        skipSpaces(b, pos);
        
        // Parse file number
        auto fileNumRes = ev.evaluate(b, pos, env);
        pos = fileNumRes.nextPos;
        int fileNumber = expr::ExpressionEvaluator::toInt16(fileNumRes.value);
        
        if (fileNumber < 1 || fileNumber > 255) {
            throwBasicError(52, "Bad file number", pos);
        }
        
        skipSpaces(b, pos);
        
        // Optional record number
        size_t recordNumber = 0;
        if (!atEnd(b, pos) && (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ","))) {
            ++pos; // consume comma
            skipSpaces(b, pos);
            
            if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                auto recNumRes = ev.evaluate(b, pos, env);
                pos = recNumRes.nextPos;
                recordNumber = static_cast<size_t>(expr::ExpressionEvaluator::toInt16(recNumRes.value));
            }
        }
        
        // Execute file GET
        if (!fileManager.getRecord(static_cast<uint8_t>(fileNumber), recordNumber)) {
            throwBasicError(62, "Input past end", pos);
        }
        
        // Update field variables in the variable table with values from record buffer
        auto* fileHandle = fileManager.getFile(static_cast<uint8_t>(fileNumber));
        if (fileHandle) {
            for (const auto& field : fileHandle->fields) {
                std::string fieldValue = fileManager.getFieldValue(static_cast<uint8_t>(fileNumber), field.name);
                gwbasic::StrDesc strDesc;
                if (strHeap.allocCopy(fieldValue.c_str(), strDesc)) {
                    vars.assignString(field.name, strDesc);
                }
            }
        }
        
        return 0;
    }
    
    uint16_t doGraphicsGET(const std::vector<uint8_t>& b, size_t& pos) {
        // GET [STEP] (x1,y1)-(x2,y2), arrayvar
        skipSpaces(b, pos);
        debugDumpTokens("GETG:entry", b, pos);
        
        // Check for optional STEP keyword
        bool stepMode = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80 && tok && tok->getTokenName(t) == "STEP") {
                stepMode = true;
                ++pos;
                skipSpaces(b, pos);
                debugDumpTokens("GETG:after-STEP", b, pos);
            }
        }
        
        // Parse first corner
        if (!consumeSymbol(b, pos, '(', "(")) {
            debugDumpTokens("GET:expected-lparen-1", b, pos);
            throwBasicError(2, "Expected ( in GET statement", pos);
        }
        
        skipSpaces(b, pos);
        
        auto x1Res = ev.evaluate(b, pos, env);
        pos = x1Res.nextPos;
        int x1 = expr::ExpressionEvaluator::toInt16(x1Res.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma in GET coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        auto y1Res = ev.evaluate(b, pos, env);
        pos = y1Res.nextPos;
        int y1 = expr::ExpressionEvaluator::toInt16(y1Res.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ')', ")")) {
            debugDumpTokens("GET:expected-rparen-1", b, pos);
            throwBasicError(2, "Expected ) in GET coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        // Expect - (ASCII or tokenized)
        if (!isSymbolAt(b, pos, '-', "-")) {
            debugDumpTokens("GET:expected-dash", b, pos);
            throwBasicError(2, "Expected - in GET statement", pos);
        }
        ++pos; // consume -
        
        skipSpaces(b, pos);
        
        // Parse second corner
        if (!consumeSymbol(b, pos, '(', "(")) {
            debugDumpTokens("GET:expected-lparen-2", b, pos);
            throwBasicError(2, "Expected ( for second corner in GET", pos);
        }
        
        skipSpaces(b, pos);
        
        auto x2Res = ev.evaluate(b, pos, env);
        pos = x2Res.nextPos;
        int x2 = expr::ExpressionEvaluator::toInt16(x2Res.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma in GET second coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        auto y2Res = ev.evaluate(b, pos, env);
        pos = y2Res.nextPos;
        int y2 = expr::ExpressionEvaluator::toInt16(y2Res.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ')', ")")) {
            debugDumpTokens("GET:expected-rparen-2", b, pos);
            throwBasicError(2, "Expected ) in GET second coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        // Expect comma before array variable
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma before array in GET", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse array variable name
        std::string arrayName = readIdentifier(b, pos);
        if (arrayName.empty()) {
            throwBasicError(2, "Expected array variable name in GET", pos);
        }
        
        // Initialize graphics context if needed
        initializeGraphicsContext();
        
        // Get pixel data
        std::vector<uint8_t> pixelData;
        if (!graphics.getBlock(x1, y1, x2, y2, pixelData, stepMode)) {
            throwBasicError(5, "Illegal function call in GET", pos);
        }
        
        // Write into an existing 1-D numeric array. Accept Int16, Single, or Double.
        // Validate array exists and is 1-D numeric.
        gwbasic::ValueType vt{}; uint8_t rank{}; std::vector<gwbasic::Dim> dims;
        if (!arrayManager.getArrayInfo(arrayName, vt, rank, dims)) {
            throwBasicError(9, "Subscript out of range or array not DIM'd", pos);
        }
        if (rank != 1) {
            throwBasicError(9, "Type mismatch: GET requires a 1-D numeric array", pos);
        }
        if (!(vt == gwbasic::ValueType::Int16 || vt == gwbasic::ValueType::Single || vt == gwbasic::ValueType::Double)) {
            throwBasicError(13, "Type mismatch: GET array must be numeric (INTEGER/SINGLE/DOUBLE)", pos);
        }
        // Compute capacity and lower bound
        int32_t lb = static_cast<int32_t>(dims[0].lb);
        int32_t ub = static_cast<int32_t>(dims[0].ub);
        int64_t capacity = static_cast<int64_t>(ub) - static_cast<int64_t>(lb) + 1;
        if (capacity < static_cast<int64_t>(pixelData.size())) {
            throwBasicError(5, "Illegal function call: array too small for GET block", pos);
        }
        // Store pixel data elements with type conversion
        for (size_t i = 0; i < pixelData.size(); i++) {
            std::vector<int32_t> indices = { static_cast<int32_t>(lb + static_cast<int32_t>(i)) };
            uint8_t byteVal = pixelData[i];
            bool ok = false;
            switch (vt) {
                case gwbasic::ValueType::Int16:
                    ok = vars.setArrayElement(arrayName, indices, gwbasic::Value::makeInt(static_cast<int16_t>(byteVal)));
                    break;
                case gwbasic::ValueType::Single:
                    ok = vars.setArrayElement(arrayName, indices, gwbasic::Value::makeSingle(static_cast<float>(byteVal)));
                    break;
                case gwbasic::ValueType::Double:
                    ok = vars.setArrayElement(arrayName, indices, gwbasic::Value::makeDouble(static_cast<double>(byteVal)));
                    break;
                default: ok = false; break;
            }
            if (!ok) {
                throwBasicError(13, "Type mismatch writing GET data", pos);
            }
        }
        
        return 0;
    }
    
    uint16_t doPUT(const std::vector<uint8_t>& b, size_t& pos) {
        debugDumpTokens("PUT:entry", b, pos);
        skipSpaces(b, pos);
        
        // Check for file PUT: PUT #filenumber, [recordnumber]
        if (!atEnd(b, pos) && (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            return doFilePUT(b, pos);
        }
        
        // Check if it looks like a graphics PUT by looking for opening parenthesis
        // Graphics PUT syntax: PUT [STEP] (x,y), arrayvar [,actionverb]
        size_t lookAhead = pos;
        skipSpaces(b, lookAhead);
        
        // Skip optional STEP keyword
        if (!atEnd(b, lookAhead)) {
            uint8_t t = b[lookAhead];
            if (t >= 0x80 && tok && tok->getTokenName(t) == "STEP") {
                ++lookAhead;
                skipSpaces(b, lookAhead);
            }
        }
        
        // If we find an opening parenthesis, assume graphics PUT
        if (!atEnd(b, lookAhead) && isSymbolAt(b, lookAhead, '(', "(")) {
            debugDumpTokens("PUT:route-graphics", b, pos);
            return doGraphicsPUT(b, pos);
        }
        
        // Default to file PUT if syntax is ambiguous
        return doFilePUT(b, pos);
    }
    
    uint16_t doFilePUT(const std::vector<uint8_t>& b, size_t& pos) {
        // PUT #filenumber, [recordnumber]
        
        // Consume the '#' token
        if (!atEnd(b, pos) && (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            ++pos;
        }
        
        skipSpaces(b, pos);
        
        // Parse file number
        auto fileNumRes = ev.evaluate(b, pos, env);
        pos = fileNumRes.nextPos;
        int fileNumber = expr::ExpressionEvaluator::toInt16(fileNumRes.value);
        
        if (fileNumber < 1 || fileNumber > 255) {
            throwBasicError(52, "Bad file number", pos);
        }
        
        skipSpaces(b, pos);
        
        // Optional record number
        size_t recordNumber = 0;
        if (!atEnd(b, pos) && (b[pos] == ',' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ","))) {
            ++pos; // consume comma
            skipSpaces(b, pos);
            
            if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                auto recNumRes = ev.evaluate(b, pos, env);
                pos = recNumRes.nextPos;
                recordNumber = static_cast<size_t>(expr::ExpressionEvaluator::toInt16(recNumRes.value));
            }
        }
        
        // Execute file PUT
        if (!fileManager.putRecord(static_cast<uint8_t>(fileNumber), recordNumber)) {
            throwBasicError(70, "Permission denied", pos);
        }
        
        return 0;
    }
    
    uint16_t doGraphicsPUT(const std::vector<uint8_t>& b, size_t& pos) {
        // PUT [STEP] (x,y), arrayvar [,actionverb]
        skipSpaces(b, pos);
        debugDumpTokens("PUTG:entry", b, pos);
        
        // Check for optional STEP keyword
        bool stepMode = false;
        if (!atEnd(b, pos)) {
            uint8_t t = b[pos];
            if (t >= 0x80 && tok && tok->getTokenName(t) == "STEP") {
                stepMode = true;
                ++pos;
                skipSpaces(b, pos);
                debugDumpTokens("PUTG:after-STEP", b, pos);
            }
        }
        
        // Parse position
        if (!consumeSymbol(b, pos, '(', "(")) {
            throwBasicError(2, "Expected ( in PUT statement", pos);
        }
        
        skipSpaces(b, pos);
        
        auto xRes = ev.evaluate(b, pos, env);
        pos = xRes.nextPos;
        int x = expr::ExpressionEvaluator::toInt16(xRes.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma in PUT coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        auto yRes = ev.evaluate(b, pos, env);
        pos = yRes.nextPos;
        int y = expr::ExpressionEvaluator::toInt16(yRes.value);
        
        skipSpaces(b, pos);
        
        if (!consumeSymbol(b, pos, ')', ")")) {
            throwBasicError(2, "Expected ) in PUT coordinates", pos);
        }
        
        skipSpaces(b, pos);
        
        // Expect comma before array variable
        if (!consumeSymbol(b, pos, ',', ",")) {
            throwBasicError(2, "Expected comma before array in PUT", pos);
        }
        
        skipSpaces(b, pos);
        
        // Parse array variable name
        std::string arrayName = readIdentifier(b, pos);
        if (arrayName.empty()) {
            throwBasicError(2, "Expected array variable name in PUT", pos);
        }
        
        skipSpaces(b, pos);
        
        // Optional action verb (PSET, PRESET, AND, OR, XOR)
        std::string mode = "PSET"; // default mode
        if (!atEnd(b, pos) && isSymbolAt(b, pos, ',', ",")) {
            ++pos; // consume comma
            skipSpaces(b, pos);
            
            if (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
                std::string actionWord;
                while (!atEnd(b, pos) && std::isalpha(static_cast<unsigned char>(b[pos]))) {
                    actionWord.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(b[pos++]))));
                }
                
                if (actionWord == "PSET" || actionWord == "PRESET" || 
                    actionWord == "AND" || actionWord == "OR" || actionWord == "XOR") {
                    mode = actionWord;
                }
            }
        }
        
        // Get pixel data from an existing 1-D numeric array (Int16/Single/Double)
        std::vector<uint8_t> pixelData;
        gwbasic::ValueType vt{}; uint8_t rank{}; std::vector<gwbasic::Dim> dims;
        if (!arrayManager.getArrayInfo(arrayName, vt, rank, dims)) {
            throwBasicError(9, "Subscript out of range or array not DIM'd", pos);
        }
        if (rank != 1) {
            throwBasicError(9, "Type mismatch: PUT requires a 1-D numeric array", pos);
        }
        if (!(vt == gwbasic::ValueType::Int16 || vt == gwbasic::ValueType::Single || vt == gwbasic::ValueType::Double)) {
            throwBasicError(13, "Type mismatch: PUT array must be numeric (INTEGER/SINGLE/DOUBLE)", pos);
        }
        int32_t lb = static_cast<int32_t>(dims[0].lb);
        int32_t ub = static_cast<int32_t>(dims[0].ub);
        int64_t count = static_cast<int64_t>(ub) - static_cast<int64_t>(lb) + 1;
        if (count < 4) {
            throwBasicError(5, "Illegal function call: PUT array too small (missing header)", pos);
        }
        // Read entire array into bytes (clamped to 0..255)
        pixelData.reserve(static_cast<size_t>(count));
        for (int64_t i = 0; i < count; ++i) {
            std::vector<int32_t> idx = { static_cast<int32_t>(lb + static_cast<int32_t>(i)) };
            gwbasic::Value v{};
            if (!vars.getArrayElement(arrayName, idx, v)) {
                throwBasicError(9, "Subscript out of range while reading PUT array", pos);
            }
            uint8_t bval = 0;
            switch (vt) {
                case gwbasic::ValueType::Int16:   bval = static_cast<uint8_t>(v.i & 0xFF); break;
                case gwbasic::ValueType::Single:  bval = static_cast<uint8_t>(static_cast<int>(std::round(v.f)) & 0xFF); break;
                case gwbasic::ValueType::Double:  bval = static_cast<uint8_t>(static_cast<int>(std::llround(v.d)) & 0xFF); break;
                default: break;
            }
            pixelData.push_back(bval);
        }
        // Validate header-driven size (LE: width, height)
        if (pixelData.size() < 4) {
            throwBasicError(5, "Illegal function call: PUT header missing", pos);
        }
        int width = static_cast<int>(pixelData[0]) | (static_cast<int>(pixelData[1]) << 8);
        int height = static_cast<int>(pixelData[2]) | (static_cast<int>(pixelData[3]) << 8);
        int64_t needed = static_cast<int64_t>(4) + static_cast<int64_t>(width) * static_cast<int64_t>(height);
        if (needed > static_cast<int64_t>(pixelData.size())) {
            throwBasicError(5, "Illegal function call: PUT data truncated for specified width/height", pos);
        }
        // Trim to exact size expected by putBlock
        pixelData.resize(static_cast<size_t>(needed));
        
        // Initialize graphics context if needed
        initializeGraphicsContext();
        
        // Execute PUT
        if (!graphics.putBlock(x, y, pixelData, mode.c_str(), stepMode)) {
            throwBasicError(5, "Illegal function call in PUT", pos);
        }
        
        return 0;
    }
    
    void initializeGraphicsContext() {
        if (graphicsBufferCallback) {
            uint8_t* buffer = graphicsBufferCallback();
            if (buffer) {
                // Use last known screen mode if available; otherwise default to mode 1
                int mode = (currentGraphicsMode_ >= 0) ? currentGraphicsMode_ : 1;
                graphics.setMode(mode, buffer);
                graphics.setDefaultColor(7); // Default to white
            }
        }
    }
    
    uint16_t doFIELD(const std::vector<uint8_t>& b, size_t& pos) {
        // FIELD [#]filenumber, fieldwidth AS string$[, fieldwidth AS string$]...
        skipSpaces(b, pos);
        
        // Optional # prefix (ASCII or tokenized)
        if (!atEnd(b, pos) && (b[pos] == '#' || (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == "#"))) {
            ++pos; // consume #
            skipSpaces(b, pos);
        }
        
        // Parse file number
        auto fileNumRes = ev.evaluate(b, pos, env);
        pos = fileNumRes.nextPos;
        int fileNumber = expr::ExpressionEvaluator::toInt16(fileNumRes.value);
        
        if (fileNumber < 1 || fileNumber > 255) {
            throwBasicError(52, "Bad file number", pos);
        }
        
        skipSpaces(b, pos);
        
        // Expect comma (accept ASCII or tokenized comma variants)
        if (atEnd(b, pos) || !(
            b[pos] == ',' || b[pos] == 0xF5 ||
            (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")
        )) {
            throwBasicError(2, "Expected comma after file number in FIELD", pos);
        }
        ++pos; // consume comma
        
        skipSpaces(b, pos);
        
        // Parse field definitions
        std::vector<std::pair<size_t, std::string>> fieldDefs;
        
        while (!atEnd(b, pos) && b[pos] != ':' && b[pos] != 0x00) {
            // Parse field width
            auto widthRes = ev.evaluate(b, pos, env);
            pos = widthRes.nextPos;
            int width = expr::ExpressionEvaluator::toInt16(widthRes.value);
            
            if (width <= 0) {
                throwBasicError(5, "Illegal function call", pos);
            }
            
            skipSpaces(b, pos);
            
            // Expect AS keyword (tokenized or ASCII)
            bool hasAs = false;
            if (!atEnd(b, pos)) {
                uint8_t t = b[pos];
                if (t >= 0x80 && tok && tok->getTokenName(t) == "AS") {
                    ++pos;
                    hasAs = true;
                } else if (t >= 0x20 && t < 0x80) {
                    // Try ASCII "AS"
                    auto save = pos;
                    std::string asWord;
                    for (int i = 0; i < 2 && !atEnd(b, pos) && b[pos] >= 0x20 && b[pos] < 0x80; ++i) {
                        asWord.push_back(static_cast<char>(std::toupper(b[pos++])));
                    }
                    if (asWord == "AS") hasAs = true; else pos = save;
                }
            }
            
            if (!hasAs) {
                throwBasicError(2, "Expected AS in FIELD statement", pos);
            }
            
            skipSpaces(b, pos);
            
            // Parse string variable name
            std::string fieldName = readIdentifier(b, pos);
            if (fieldName.empty()) {
                throwBasicError(2, "Expected variable name in FIELD", pos);
            }
            
            // Add string suffix if not present
            if (fieldName.back() != '$') {
                fieldName += '$';
            }
            
            fieldDefs.emplace_back(static_cast<size_t>(width), fieldName);
            
            skipSpaces(b, pos);
            
            // Check for continuation
            if (!atEnd(b, pos) && (
                b[pos] == ',' || b[pos] == 0xF5 ||
                (b[pos] >= 0x80 && tok && tok->getTokenName(b[pos]) == ",")
            )) {
                ++pos; // consume comma
                skipSpaces(b, pos);
            } else {
                break;
            }
        }
        
        // Execute FIELD
        if (!fileManager.fieldFile(static_cast<uint8_t>(fileNumber), fieldDefs)) {
            throwBasicError(52, "Bad file number", pos);
        }
        
        // Initialize all field variables as empty strings in the variable table
        // and track which file they belong to
        for (const auto& fieldDef : fieldDefs) {
            const std::string& varName = fieldDef.second;
            vars.createString(varName, "");  // Use existing createString method
            fieldToFileMap[varName] = static_cast<uint8_t>(fileNumber);
        }
        
        return 0;
    }
    
    uint16_t doLSET(const std::vector<uint8_t>& b, size_t& pos) {
        // LSET string$ = expression
        skipSpaces(b, pos);
        
        // Parse variable name
        std::string varName = readIdentifier(b, pos);
        if (varName.empty()) {
            throwBasicError(2, "Expected variable name in LSET", pos);
        }
        
        // Add string suffix if not present
        if (varName.back() != '$') {
            varName += '$';
        }
        
        skipSpaces(b, pos);
        
        // Expect = or tokenized equals (various formats: 0xD1, 0xD2, 0xE5, 0xE6)
        if (atEnd(b, pos) || !(b[pos] == '=' || b[pos] == 0xD1 || b[pos] == 0xD2 || b[pos] == 0xE5 || b[pos] == 0xE6)) {
            throwBasicError(2, "Expected = in LSET statement", pos);
        }
        ++pos; // consume =
        
        skipSpaces(b, pos);
        
        // Parse expression
        auto valueRes = ev.evaluate(b, pos, env);
        pos = valueRes.nextPos;
        
        std::string value;
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Str>) {
                value = x.v;
            } else {
                // Convert numeric to string
                if constexpr (std::is_same_v<T, expr::Int16>) {
                    value = std::to_string(x.v);
                } else if constexpr (std::is_same_v<T, expr::Single>) {
                    value = std::to_string(x.v);
                } else if constexpr (std::is_same_v<T, expr::Double>) {
                    value = std::to_string(x.v);
                }
            }
        }, valueRes.value);
        
        // Find which file this field belongs to and execute LSET
        auto it = fieldToFileMap.find(varName);
        if (it == fieldToFileMap.end()) {
            throwBasicError(2, "Variable not a field variable", pos);
        }
        uint8_t fileNum = it->second;
        
        if (!fileManager.lsetField(fileNum, varName, value)) {
            throwBasicError(52, "Bad file number", pos);
        }
        
        // Also set the variable in the variable table so it can be accessed
        gwbasic::StrDesc strDesc;
        if (strHeap.allocCopy(value.c_str(), strDesc)) {
            vars.assignString(varName, strDesc);
        }
        
        return 0;
    }
    
    uint16_t doRSET(const std::vector<uint8_t>& b, size_t& pos) {
        // RSET string$ = expression
        skipSpaces(b, pos);
        
        // Parse variable name
        std::string varName = readIdentifier(b, pos);
        if (varName.empty()) {
            throwBasicError(2, "Expected variable name in RSET", pos);
        }
        
        // Add string suffix if not present
        if (varName.back() != '$') {
            varName += '$';
        }
        
        skipSpaces(b, pos);
        
        // Expect = or tokenized equals (various formats: 0xD1, 0xD2, 0xE5, 0xE6)
        if (atEnd(b, pos) || !(b[pos] == '=' || b[pos] == 0xD1 || b[pos] == 0xD2 || b[pos] == 0xE5 || b[pos] == 0xE6)) {
            throwBasicError(2, "Expected = in RSET statement", pos);
        }
        ++pos; // consume =
        
        skipSpaces(b, pos);
        
        // Parse expression
        auto valueRes = ev.evaluate(b, pos, env);
        pos = valueRes.nextPos;
        
        std::string value;
        std::visit([&](auto&& x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (std::is_same_v<T, expr::Str>) {
                value = x.v;
            } else {
                // Convert numeric to string
                if constexpr (std::is_same_v<T, expr::Int16>) {
                    value = std::to_string(x.v);
                } else if constexpr (std::is_same_v<T, expr::Single>) {
                    value = std::to_string(x.v);
                } else if constexpr (std::is_same_v<T, expr::Double>) {
                    value = std::to_string(x.v);
                }
            }
        }, valueRes.value);
        
        // Find which file this field belongs to and execute RSET
        auto it = fieldToFileMap.find(varName);
        if (it == fieldToFileMap.end()) {
            throwBasicError(2, "Variable not a field variable", pos);
        }
        uint8_t fileNum = it->second;
        
        if (!fileManager.rsetField(fileNum, varName, value)) {
            throwBasicError(52, "Bad file number", pos);
        }
        
        // Also set the variable in the variable table so it can be accessed
        gwbasic::StrDesc strDesc;
        if (strHeap.allocCopy(value.c_str(), strDesc)) {
            vars.assignString(varName, strDesc);
        }
        
        return 0;
    }
    
private:
    // Music string parser and player for PLAY command
    void playMusicString(const std::string& musicString) {
        // For now, just parse the music string and print what would be played
        // In the future, this will generate actual audio
        
        if (printCallback) {
            printCallback("PLAY: \"" + musicString + "\"\n");
        }
        
        // Parse basic music notation
        parseMusicNotation(musicString);
    }
    
    void parseMusicNotation(const std::string& music) {
        // Basic parser for GW-BASIC music notation
        // Notes: A, B, C, D, E, F, G (with optional # or -)
        // Octave: O followed by number (0-6)
        // Length: L followed by number (1-64)
        // Tempo: T followed by number (32-255)
        // Pause: P followed by number
        
        size_t pos = 0;
        int currentOctave = 4;  // Default octave
        int currentLength = 4;  // Default note length (quarter note)
        int currentTempo = 120; // Default tempo
        
        while (pos < music.length()) {
            char c = std::toupper(music[pos]);
            
            switch (c) {
                case 'A': case 'B': case 'C': case 'D': case 'E': case 'F': case 'G': {
                    // Note with optional sharp/flat
                    pos++;
                    bool sharp = false, flat = false;
                    if (pos < music.length() && music[pos] == '#') {
                        sharp = true;
                        pos++;
                    } else if (pos < music.length() && music[pos] == '-') {
                        flat = true;
                        pos++;
                    }
                    
                    // Parse optional length
                    int noteLength = currentLength;
                    if (pos < music.length() && std::isdigit(music[pos])) {
                        noteLength = 0;
                        while (pos < music.length() && std::isdigit(music[pos])) {
                            noteLength = noteLength * 10 + (music[pos] - '0');
                            pos++;
                        }
                    }
                    
                    playNote(c, sharp, flat, currentOctave, noteLength, currentTempo);
                    break;
                }
                
                case 'O': {
                    // Octave
                    pos++;
                    if (pos < music.length() && std::isdigit(music[pos])) {
                        currentOctave = music[pos] - '0';
                        if (currentOctave < 0 || currentOctave > 6) {
                            currentOctave = 4; // Reset to default on invalid octave
                        }
                        pos++;
                    }
                    break;
                }
                
                case 'L': {
                    // Length
                    pos++;
                    if (pos < music.length() && std::isdigit(music[pos])) {
                        currentLength = 0;
                        while (pos < music.length() && std::isdigit(music[pos])) {
                            currentLength = currentLength * 10 + (music[pos] - '0');
                            pos++;
                        }
                        if (currentLength < 1 || currentLength > 64) {
                            currentLength = 4; // Reset to default on invalid length
                        }
                    }
                    break;
                }
                
                case 'T': {
                    // Tempo
                    pos++;
                    if (pos < music.length() && std::isdigit(music[pos])) {
                        currentTempo = 0;
                        while (pos < music.length() && std::isdigit(music[pos])) {
                            currentTempo = currentTempo * 10 + (music[pos] - '0');
                            pos++;
                        }
                        if (currentTempo < 32 || currentTempo > 255) {
                            currentTempo = 120; // Reset to default on invalid tempo
                        }
                    }
                    break;
                }
                
                case 'P': {
                    // Pause
                    pos++;
                    int pauseLength = currentLength;
                    if (pos < music.length() && std::isdigit(music[pos])) {
                        pauseLength = 0;
                        while (pos < music.length() && std::isdigit(music[pos])) {
                            pauseLength = pauseLength * 10 + (music[pos] - '0');
                            pos++;
                        }
                    }
                    playPause(pauseLength, currentTempo);
                    break;
                }
                
                case ' ': case '\t': case '\n': case '\r': {
                    // Skip whitespace
                    pos++;
                    break;
                }
                
                default: {
                    // Unknown character, skip it
                    pos++;
                    break;
                }
            }
        }
    }
    
    void playNote(char note, bool sharp, bool flat, int octave, int length, int tempo) {
        // Calculate frequency for the note
        double frequency = getNoteFrequency(note, sharp, flat, octave);
        
        // Calculate duration based on length and tempo
        // Length is note value (1=whole, 2=half, 4=quarter, etc.)
        // Tempo is in quarter notes per minute
        int durationMs = calculateNoteDuration(length, tempo);
        
        if (soundCallback) {
            soundCallback(frequency, durationMs);
        } else if (printCallback) {
            // Fallback to text output if no sound system
            std::string noteStr = "";
            noteStr += note;
            if (sharp) noteStr += "#";
            if (flat) noteStr += "-";
            
            printCallback("Note: " + noteStr + " Octave: " + std::to_string(octave) + 
                         " Freq: " + std::to_string(frequency) + "Hz Duration: " + std::to_string(durationMs) + "ms\n");
        }
    }
    
    void playPause(int length, int tempo) {
        // Calculate pause duration
        int durationMs = calculateNoteDuration(length, tempo);
        
        if (soundCallback) {
            // Pause is just silence (0 frequency)
            soundCallback(0.0, durationMs);
        } else if (printCallback) {
            printCallback("Pause: Duration: " + std::to_string(durationMs) + "ms\n");
        }
    }
    
    double getNoteFrequency(char note, bool sharp, bool flat, int octave) {
        // Base frequencies for notes in octave 4 (middle octave)
        double baseFreq = 0.0;
        
        switch (note) {
            case 'C': baseFreq = 261.63; break;
            case 'D': baseFreq = 293.66; break;
            case 'E': baseFreq = 329.63; break;
            case 'F': baseFreq = 349.23; break;
            case 'G': baseFreq = 392.00; break;
            case 'A': baseFreq = 440.00; break;
            case 'B': baseFreq = 493.88; break;
            default: return 0.0; // Invalid note
        }
        
        // Apply sharp/flat
        if (sharp) {
            baseFreq *= 1.0594630943593; // 2^(1/12) - semitone up
        } else if (flat) {
            baseFreq /= 1.0594630943593; // semitone down
        }
        
        // Adjust for octave (each octave doubles or halves frequency)
        int octaveDiff = octave - 4;
        if (octaveDiff > 0) {
            baseFreq *= (1 << octaveDiff); // multiply by 2^octaveDiff
        } else if (octaveDiff < 0) {
            baseFreq /= (1 << (-octaveDiff)); // divide by 2^abs(octaveDiff)
        }
        
        return baseFreq;
    }
    
    int calculateNoteDuration(int length, int tempo) {
        // Calculate duration in milliseconds
        // length: 1=whole note, 4=quarter note, etc.
        // tempo: quarter notes per minute
        
        // Duration of a quarter note in milliseconds
        double quarterNoteDuration = 60000.0 / tempo;
        
        // Duration of this note (quarter note duration * 4 / length)
        double noteDuration = quarterNoteDuration * 4.0 / length;
        
        return static_cast<int>(noteDuration);
    }
    
    // DRAW statement implementation
    void executeDRAWString(const std::string& drawString) {
        size_t pos = 0;
        
        while (pos < drawString.length()) {
            char cmd = std::toupper(drawString[pos]);
            ++pos;
            
            // Skip spaces
            while (pos < drawString.length() && std::isspace(drawString[pos])) {
                ++pos;
            }
            
            switch (cmd) {
                case 'U': // Move up
                    executeDRAWMove(0, -1, parseDrawParameter(drawString, pos));
                    break;
                case 'D': // Move down  
                    executeDRAWMove(0, 1, parseDrawParameter(drawString, pos));
                    break;
                case 'L': // Move left
                    executeDRAWMove(-1, 0, parseDrawParameter(drawString, pos));
                    break;
                case 'R': // Move right
                    executeDRAWMove(1, 0, parseDrawParameter(drawString, pos));
                    break;
                case 'E': // Move diagonally down and right
                    executeDRAWMove(1, 1, parseDrawParameter(drawString, pos));
                    break;
                case 'F': // Move diagonally up and right
                    executeDRAWMove(1, -1, parseDrawParameter(drawString, pos));
                    break;
                case 'G': // Move diagonally up and left
                    executeDRAWMove(-1, -1, parseDrawParameter(drawString, pos));
                    break;
                case 'H': // Move diagonally down and left
                    executeDRAWMove(-1, 1, parseDrawParameter(drawString, pos));
                    break;
                case 'M': // Move to absolute or relative position
                    executeDRAWMoveCommand(drawString, pos);
                    break;
                case 'A': // Set angle
                    executeDRAWAngleCommand(drawString, pos);
                    break;
                case 'B': // Blank move (move without drawing)
                    drawFlags |= 0x80; // Set no-plot flag
                    break;
                case 'N': // No move (draw without moving)
                    drawFlags |= 0x40; // Set no-move flag
                    break;
                case 'C': // Set color
                    executeDRAWColorCommand(drawString, pos);
                    break;
                case 'S': // Set scale
                    executeDRAWScaleCommand(drawString, pos);
                    break;
                case 'X': // Execute substring
                    executeDRAWExecuteCommand(drawString, pos);
                    break;
                case ' ': // Skip spaces
                case '\t':
                    break;
                default:
                    // Ignore unknown commands for compatibility
                    break;
            }
        }
    }
    
    int parseDrawParameter(const std::string& drawString, size_t& pos) {
        int value = 1; // Default value
        bool negative = false;
        
        // Skip spaces
        while (pos < drawString.length() && std::isspace(drawString[pos])) {
            ++pos;
        }
        
        if (pos < drawString.length()) {
            // Check for sign
            if (drawString[pos] == '+') {
                ++pos;
            } else if (drawString[pos] == '-') {
                negative = true;
                ++pos;
            }
            
            // Parse number
            if (pos < drawString.length() && std::isdigit(drawString[pos])) {
                value = 0;
                while (pos < drawString.length() && std::isdigit(drawString[pos])) {
                    value = value * 10 + (drawString[pos] - '0');
                    ++pos;
                }
            }
        }
        
        return negative ? -value : value;
    }
    
    std::pair<int, int> transformDRAWCoordinates(int dx, int dy) {
        // Apply angle rotation (0=0°, 1=90°, 2=180°, 3=270°)
        int transformedDx = dx, transformedDy = dy;
        
        switch (drawAngle) {
            case 0: // 0° - no rotation
                break;
            case 1: // 90° clockwise: (x,y) -> (y,-x)
                transformedDx = dy;
                transformedDy = -dx;
                break;
            case 2: // 180°: (x,y) -> (-x,-y)
                transformedDx = -dx;
                transformedDy = -dy;
                break;
            case 3: // 270° clockwise: (x,y) -> (-y,x)
                transformedDx = -dy;
                transformedDy = dx;
                break;
        }
        
        // Apply scaling if enabled
        if (drawScale > 0) {
            transformedDx = (transformedDx * drawScale) / 4;
            transformedDy = (transformedDy * drawScale) / 4;
        }
        
        return {transformedDx, transformedDy};
    }
    
    void executeDRAWMove(int dirX, int dirY, int distance) {
        auto [dx, dy] = transformDRAWCoordinates(dirX * distance, dirY * distance);
        
        // Get current graphics position
        auto [currentX, currentY] = graphics.getCurrentPoint();
        int newX = currentX + dx;
        int newY = currentY + dy;
        
        bool shouldDraw = !(drawFlags & 0x80); // Check no-plot flag
        bool shouldMove = !(drawFlags & 0x40);  // Check no-move flag
        
        if (shouldDraw) {
            // Draw line from current position to new position
            graphics.line(currentX, currentY, newX, newY);
        }
        
        if (shouldMove) {
            // Update current position
            graphics.setCurrentPoint(newX, newY);
        }
        
        // Clear drawing flags after use
        drawFlags = 0;
    }
    
    void executeDRAWMoveCommand(const std::string& drawString, size_t& pos) {
        // Skip spaces
        while (pos < drawString.length() && std::isspace(drawString[pos])) {
            ++pos;
        }
        
        bool relative = false;
        if (pos < drawString.length() && (drawString[pos] == '+' || drawString[pos] == '-')) {
            relative = true;
        }
        
        int x = parseDrawParameter(drawString, pos);
        
        // Expect comma
        while (pos < drawString.length() && std::isspace(drawString[pos])) {
            ++pos;
        }
        if (pos < drawString.length() && drawString[pos] == ',') {
            ++pos;
        }
        
        int y = parseDrawParameter(drawString, pos);
        
        auto [currentX, currentY] = graphics.getCurrentPoint();
        int targetX, targetY;
        
        if (relative) {
            auto [dx, dy] = transformDRAWCoordinates(x, y);
            targetX = currentX + dx;
            targetY = currentY + dy;
        } else {
            targetX = x;
            targetY = y;
        }
        
        bool shouldDraw = !(drawFlags & 0x80);
        bool shouldMove = !(drawFlags & 0x40);
        
        if (shouldDraw) {
            graphics.line(currentX, currentY, targetX, targetY);
        }
        
        if (shouldMove) {
            graphics.setCurrentPoint(targetX, targetY);
        }
        
        drawFlags = 0;
    }
    
    void executeDRAWAngleCommand(const std::string& drawString, size_t& pos) {
        int angle = parseDrawParameter(drawString, pos);
        if (angle >= 0 && angle <= 3) {
            drawAngle = static_cast<uint8_t>(angle);
        }
    }
    
    void executeDRAWColorCommand(const std::string& drawString, size_t& pos) {
        int color = parseDrawParameter(drawString, pos);
        if (color >= 0 && color <= 15) {
            graphics.setDefaultColor(static_cast<uint8_t>(color));
        }
    }
    
    void executeDRAWScaleCommand(const std::string& drawString, size_t& pos) {
        int scale = parseDrawParameter(drawString, pos);
        if (scale >= 0 && scale <= 255) {
            drawScale = static_cast<uint8_t>(scale);
        }
    }
    
    void executeDRAWExecuteCommand(const std::string& drawString, size_t& pos) {
        // X command should execute a string variable
        // For now, just skip the parameter as this requires variable lookup
        // This is a simplified implementation
        parseDrawParameter(drawString, pos);
    }

public:
};
