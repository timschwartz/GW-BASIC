// String function integration layer implementation
#include "StringFunctions.hpp"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <limits>
#include <cmath>

namespace gwbasic {

gwbasic::Value StringFunctionProcessor::exprToRuntime(const expr::Value& exprVal) {
    return std::visit([this](auto&& arg) -> gwbasic::Value {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, expr::Int16>) {
            return gwbasic::Value::makeInt(arg.v);
        }
        if constexpr (std::is_same_v<T, expr::Single>) {
            return gwbasic::Value::makeSingle(arg.v);
        }
        if constexpr (std::is_same_v<T, expr::Double>) {
            return gwbasic::Value::makeDouble(arg.v);
        }
        if constexpr (std::is_same_v<T, expr::Str>) {
            StrDesc desc;
            if (stringManager_->createString(arg.v, desc)) {
                return gwbasic::Value::makeString(desc);
            }
            // Memory allocation failed - return empty string
            return gwbasic::Value::makeString(StrDesc{});
        }
    }, exprVal);
}

expr::Value StringFunctionProcessor::runtimeToExpr(const gwbasic::Value& runtimeVal) {
    switch (runtimeVal.type) {
        case ScalarType::Int16:
            return expr::Int16{runtimeVal.i};
        case ScalarType::Single:
            return expr::Single{runtimeVal.f};
        case ScalarType::Double:
            return expr::Double{runtimeVal.d};
        case ScalarType::String:
            return expr::Str{stringManager_->toString(runtimeVal.s)};
        default:
            return expr::Int16{0};
    }
}

gwbasic::Value StringFunctionProcessor::chr(int16_t asciiCode) {
    if (asciiCode < 0 || asciiCode > 255) {
        throw std::runtime_error("Illegal function call: CHR$ code out of range");
    }
    
    StrDesc result;
    uint8_t chr_val = static_cast<uint8_t>(asciiCode);
    if (stringManager_->createString(&chr_val, 1, result)) {
        return gwbasic::Value::makeString(result);
    }
    
    // Memory allocation failed - return empty string
    return gwbasic::Value::makeString(StrDesc{});
}

gwbasic::Value StringFunctionProcessor::str(const gwbasic::Value& numericValue) {
    if (!isNumeric(numericValue)) {
        throw std::runtime_error("Type mismatch: STR$ requires numeric argument");
    }
    
    std::string strVal = numericToString(numericValue);
    
    // Add leading space for positive numbers (GW-BASIC behavior)
    if (!strVal.empty() && strVal[0] != '-') {
        strVal = " " + strVal;
    }
    
    StrDesc result;
    if (stringManager_->createString(strVal, result)) {
        return gwbasic::Value::makeString(result);
    }
    
    // Memory allocation failed - return empty string
    return gwbasic::Value::makeString(StrDesc{});
}

gwbasic::Value StringFunctionProcessor::left(const gwbasic::Value& source, int16_t count) {
    if (!isString(source)) {
        throw std::runtime_error("Type mismatch: LEFT$ requires string argument");
    }
    
    if (count < 0) {
        throw std::runtime_error("Illegal function call: LEFT$ count cannot be negative");
    }
    
    StrDesc result;
    if (stringManager_->left(source.s, static_cast<uint16_t>(count), result)) {
        return gwbasic::Value::makeString(result);
    }
    
    // Memory allocation failed - return empty string
    return gwbasic::Value::makeString(StrDesc{});
}

gwbasic::Value StringFunctionProcessor::right(const gwbasic::Value& source, int16_t count) {
    if (!isString(source)) {
        throw std::runtime_error("Type mismatch: RIGHT$ requires string argument");
    }
    
    if (count < 0) {
        throw std::runtime_error("Illegal function call: RIGHT$ count cannot be negative");
    }
    
    StrDesc result;
    if (stringManager_->right(source.s, static_cast<uint16_t>(count), result)) {
        return gwbasic::Value::makeString(result);
    }
    
    // Memory allocation failed - return empty string
    return gwbasic::Value::makeString(StrDesc{});
}

gwbasic::Value StringFunctionProcessor::mid(const gwbasic::Value& source, int16_t start, int16_t optCount) {
    if (!isString(source)) {
        throw std::runtime_error("Type mismatch: MID$ requires string argument");
    }
    
    if (start < 1) {
        throw std::runtime_error("Illegal function call: MID$ start position must be >= 1");
    }
    
    StrDesc result;
    if (stringManager_->mid(source.s, static_cast<uint16_t>(start), optCount, result)) {
        return gwbasic::Value::makeString(result);
    }
    
    // Memory allocation failed - return empty string
    return gwbasic::Value::makeString(StrDesc{});
}

gwbasic::Value StringFunctionProcessor::val(const gwbasic::Value& stringValue) {
    if (!isString(stringValue)) {
        throw std::runtime_error("Type mismatch: VAL requires string argument");
    }
    
    std::string str = stringManager_->toString(stringValue.s);
    
    // Trim leading spaces
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
    
    if (str.empty()) {
        return gwbasic::Value::makeInt(0);
    }
    
    return parseNumeric(str);
}

gwbasic::Value StringFunctionProcessor::string(int16_t count, const gwbasic::Value& charOrAscii) {
    if (count < 0) {
        throw std::runtime_error("Illegal function call: STRING$ count cannot be negative");
    }
    
    if (count > 255) {
        throw std::runtime_error("Illegal function call: STRING$ count too large");
    }
    
    char ch;
    if (isString(charOrAscii)) {
        // If it's a string, use the first character
        if (charOrAscii.s.len == 0) {
            throw std::runtime_error("Illegal function call: STRING$ with empty string");
        }
        std::string str = stringManager_->toString(charOrAscii.s);
        ch = str[0];
    } else if (isNumeric(charOrAscii)) {
        // If it's a number, treat it as ASCII code
        int16_t asciiCode = toInt16(charOrAscii);
        if (asciiCode < 0 || asciiCode > 255) {
            throw std::runtime_error("Illegal function call: ASCII code out of range");
        }
        ch = static_cast<char>(asciiCode);
    } else {
        throw std::runtime_error("Type mismatch: STRING$ requires string or numeric argument");
    }
    
    std::string result(count, ch);
    StrDesc desc;
    if (stringManager_->createString(result, desc)) {
        return gwbasic::Value::makeString(desc);
    }
    // Memory allocation failed - return empty string
    return gwbasic::Value::makeString(StrDesc{});
}

gwbasic::Value StringFunctionProcessor::space(int16_t count) {
    if (count < 0) {
        throw std::runtime_error("Illegal function call: SPACE$ count cannot be negative");
    }
    
    if (count > 255) {
        throw std::runtime_error("Illegal function call: SPACE$ count too large");
    }
    
    std::string result(count, ' ');
    StrDesc desc;
    if (stringManager_->createString(result, desc)) {
        return gwbasic::Value::makeString(desc);
    }
    // Memory allocation failed - return empty string
    return gwbasic::Value::makeString(StrDesc{});
}

int16_t StringFunctionProcessor::len(const gwbasic::Value& stringValue) {
    if (!isString(stringValue)) {
        throw std::runtime_error("Type mismatch: LEN requires string argument");
    }
    
    return static_cast<int16_t>(stringValue.s.len);
}

int16_t StringFunctionProcessor::asc(const gwbasic::Value& stringValue) {
    if (!isString(stringValue)) {
        throw std::runtime_error("Type mismatch: ASC requires string argument");
    }
    
    if (stringValue.s.len == 0) {
        throw std::runtime_error("Illegal function call: ASC of empty string");
    }
    
    return static_cast<int16_t>(stringValue.s.ptr[0]);
}

int16_t StringFunctionProcessor::instr(const gwbasic::Value& source, const gwbasic::Value& search, int16_t start) {
    if (!isString(source) || !isString(search)) {
        throw std::runtime_error("Type mismatch: INSTR requires string arguments");
    }
    
    if (start < 1) {
        start = 1;
    }
    
    return static_cast<int16_t>(stringManager_->instr(source.s, search.s, static_cast<uint16_t>(start)));
}

bool StringFunctionProcessor::callStringFunction(const std::string& funcName, const std::vector<expr::Value>& args, expr::Value& result) {
    // Convert function name to uppercase for comparison
    std::string upperFuncName = funcName;
    std::transform(upperFuncName.begin(), upperFuncName.end(), upperFuncName.begin(), 
                   [](unsigned char c) { return std::toupper(c); });
    
    try {
        if (upperFuncName == "CHR$" && args.size() == 1) {
            auto runtimeArg = exprToRuntime(args[0]);
            if (!isNumeric(runtimeArg)) {
                throw std::runtime_error("Type mismatch");
            }
            auto runtimeResult = chr(toInt16(runtimeArg));
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "STR$" && args.size() == 1) {
            auto runtimeArg = exprToRuntime(args[0]);
            auto runtimeResult = str(runtimeArg);
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "LEFT$" && args.size() == 2) {
            auto runtimeArg1 = exprToRuntime(args[0]);
            auto runtimeArg2 = exprToRuntime(args[1]);
            if (!isNumeric(runtimeArg2)) {
                throw std::runtime_error("Type mismatch");
            }
            auto runtimeResult = left(runtimeArg1, toInt16(runtimeArg2));
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "RIGHT$" && args.size() == 2) {
            auto runtimeArg1 = exprToRuntime(args[0]);
            auto runtimeArg2 = exprToRuntime(args[1]);
            if (!isNumeric(runtimeArg2)) {
                throw std::runtime_error("Type mismatch");
            }
            auto runtimeResult = right(runtimeArg1, toInt16(runtimeArg2));
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "MID$" && (args.size() == 2 || args.size() == 3)) {
            auto runtimeArg1 = exprToRuntime(args[0]);
            auto runtimeArg2 = exprToRuntime(args[1]);
            if (!isNumeric(runtimeArg2)) {
                throw std::runtime_error("Type mismatch");
            }
            
            int16_t optCount = -1;
            if (args.size() == 3) {
                auto runtimeArg3 = exprToRuntime(args[2]);
                if (!isNumeric(runtimeArg3)) {
                    throw std::runtime_error("Type mismatch");
                }
                optCount = toInt16(runtimeArg3);
            }
            
            auto runtimeResult = mid(runtimeArg1, toInt16(runtimeArg2), optCount);
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "VAL" && args.size() == 1) {
            auto runtimeArg = exprToRuntime(args[0]);
            auto runtimeResult = val(runtimeArg);
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "LEN" && args.size() == 1) {
            auto runtimeArg = exprToRuntime(args[0]);
            int16_t length = len(runtimeArg);
            result = expr::Int16{length};
            return true;
        }
        
        if (upperFuncName == "ASC" && args.size() == 1) {
            auto runtimeArg = exprToRuntime(args[0]);
            int16_t asciiVal = asc(runtimeArg);
            result = expr::Int16{asciiVal};
            return true;
        }
        
        if (upperFuncName == "STRING$" && args.size() == 2) {
            auto runtimeArg1 = exprToRuntime(args[0]);
            auto runtimeArg2 = exprToRuntime(args[1]);
            if (!isNumeric(runtimeArg1)) {
                throw std::runtime_error("Type mismatch");
            }
            auto runtimeResult = string(toInt16(runtimeArg1), runtimeArg2);
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "SPACE$" && args.size() == 1) {
            auto runtimeArg = exprToRuntime(args[0]);
            if (!isNumeric(runtimeArg)) {
                throw std::runtime_error("Type mismatch");
            }
            auto runtimeResult = space(toInt16(runtimeArg));
            result = runtimeToExpr(runtimeResult);
            return true;
        }
        
        if (upperFuncName == "INSTR" && (args.size() == 2 || args.size() == 3)) {
            if (args.size() == 2) {
                auto runtimeArg1 = exprToRuntime(args[0]);
                auto runtimeArg2 = exprToRuntime(args[1]);
                int16_t pos = instr(runtimeArg1, runtimeArg2, 1);
                result = expr::Int16{pos};
                return true;
            } else {
                auto runtimeArg1 = exprToRuntime(args[0]);
                auto runtimeArg2 = exprToRuntime(args[1]);
                auto runtimeArg3 = exprToRuntime(args[2]);
                if (!isNumeric(runtimeArg3)) {
                    throw std::runtime_error("Type mismatch");
                }
                int16_t pos = instr(runtimeArg1, runtimeArg2, toInt16(runtimeArg3));
                result = expr::Int16{pos};
                return true;
            }
        }
        
    } catch (const std::exception&) {
        return false; // Let calling code handle the error
    }
    
    return false; // Function not handled by this processor
}

// Private helper methods

const StrDesc& StringFunctionProcessor::ensureString(const gwbasic::Value& val) {
    if (!isString(val)) {
        throw std::runtime_error("Type mismatch: string expected");
    }
    return val.s;
}

std::string StringFunctionProcessor::numericToString(const gwbasic::Value& val) {
    std::ostringstream oss;
    
    switch (val.type) {
        case ScalarType::Int16:
            oss << val.i;
            break;
        case ScalarType::Single:
            oss << val.f;
            break;
        case ScalarType::Double:
            oss << val.d;
            break;
        default:
            throw std::runtime_error("Type mismatch: numeric value expected");
    }
    
    return oss.str();
}

gwbasic::Value StringFunctionProcessor::parseNumeric(const std::string& str) {
    if (str.empty()) {
        return gwbasic::Value::makeInt(0);
    }
    
    try {
        // Try parsing as integer first if no decimal point or exponent
        if (str.find('.') == std::string::npos && 
            str.find('e') == std::string::npos && 
            str.find('E') == std::string::npos) {
            
            long val = std::stol(str);
            if (val >= std::numeric_limits<int16_t>::min() && 
                val <= std::numeric_limits<int16_t>::max()) {
                return gwbasic::Value::makeInt(static_cast<int16_t>(val));
            }
        }
        
        // Parse as double
        double val = std::stod(str);
        return gwbasic::Value::makeDouble(val);
        
    } catch (...) {
        return gwbasic::Value::makeInt(0); // VAL returns 0 for invalid strings
    }
}

bool StringFunctionProcessor::isNumeric(const gwbasic::Value& val) {
    return val.type == ScalarType::Int16 || 
           val.type == ScalarType::Single || 
           val.type == ScalarType::Double;
}

bool StringFunctionProcessor::isString(const gwbasic::Value& val) {
    return val.type == ScalarType::String;
}

double StringFunctionProcessor::toDouble(const gwbasic::Value& val) {
    switch (val.type) {
        case ScalarType::Int16: return static_cast<double>(val.i);
        case ScalarType::Single: return static_cast<double>(val.f);
        case ScalarType::Double: return val.d;
        default: throw std::runtime_error("Type mismatch: numeric value expected");
    }
}

int16_t StringFunctionProcessor::toInt16(const gwbasic::Value& val) {
    switch (val.type) {
        case ScalarType::Int16: return val.i;
        case ScalarType::Single: return static_cast<int16_t>(std::round(val.f));
        case ScalarType::Double: return static_cast<int16_t>(std::round(val.d));
        default: throw std::runtime_error("Type mismatch: numeric value expected");
    }
}

} // namespace gwbasic