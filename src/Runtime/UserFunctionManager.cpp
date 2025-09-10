#include "UserFunctionManager.hpp"
#include "VariableTable.hpp"
#include "../Tokenizer/Tokenizer.hpp"
#include "../ExpressionEvaluator/ExpressionEvaluator.hpp"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <variant>

namespace gwbasic {

UserFunctionManager::UserFunctionManager(StringHeap* heap, std::shared_ptr<Tokenizer> tok)
    : stringHeap(heap), tokenizer(std::move(tok)) {
}

bool UserFunctionManager::defineFunction(const std::string& name, 
                                        const std::vector<std::string>& parameters,
                                        const std::vector<uint8_t>& expression,
                                        ScalarType returnType) {
    // Normalize function name for storage
    std::string normalizedName = normalizeNameForLookup(name);
    
    // Validate parameters
    if (parameters.size() > 255) {
        return false; // Too many parameters
    }
    
    // Check for duplicate parameter names
    std::vector<std::string> normalizedParams;
    normalizedParams.reserve(parameters.size());
    for (const auto& param : parameters) {
        std::string normalized = normalizeNameForLookup(param);
        if (std::find(normalizedParams.begin(), normalizedParams.end(), normalized) != normalizedParams.end()) {
            return false; // Duplicate parameter
        }
        normalizedParams.push_back(normalized);
    }
    
    // Store function
    UserFunction func;
    func.name = normalizedName;
    func.parameters = normalizedParams;
    func.expression = expression;
    func.returnType = returnType;
    
    functions[normalizedName] = std::move(func);
    return true;
}

bool UserFunctionManager::isUserFunction(const std::string& name) const {
    std::string normalizedName = normalizeNameForLookup(name);
    return functions.find(normalizedName) != functions.end();
}

bool UserFunctionManager::callFunction(const std::string& name, 
                                      const std::vector<Value>& arguments,
                                      Value& result) const {
    std::string normalizedName = normalizeNameForLookup(name);
    auto it = functions.find(normalizedName);
    if (it == functions.end()) {
        return false; // Function not found
    }
    
    const UserFunction& func = it->second;
    
    // Check parameter count
    if (arguments.size() != func.parameters.size()) {
        return false; // Wrong number of arguments
    }
    
    // Create an ExpressionEvaluator for this function call
    if (!tokenizer) {
        return false; // No tokenizer available
    }
    
    // Cast to the correct tokenizer type (the issue seems to be a namespace mismatch)
    expr::ExpressionEvaluator evaluator(tokenizer);
    
    // Set up environment with parameter bindings
    expr::Env env;
    env.vars.clear();
    
    // Bind parameters to their argument values
    for (size_t i = 0; i < func.parameters.size(); ++i) {
        const std::string& paramName = func.parameters[i];
        const Value& argValue = arguments[i];
        
        // Store in our local environment for this function call
        currentLocals.locals[paramName] = argValue;
    }
    
    // Set up the environment lookup function
    env.getVar = [this](const std::string& varName, expr::Value& out) -> bool {
        auto localIt = currentLocals.locals.find(normalizeNameForLookup(varName));
        if (localIt != currentLocals.locals.end()) {
            out = toExprValue(localIt->second);
            return true;
        }
        return false; // Variable not found
    };
    
    // Evaluate the function expression
    auto evalResult = evaluator.evaluate(func.expression, 0, env);
    
    // Convert result back to runtime value
    result = convertToType(fromExprValue(evalResult.value), func.returnType);
    
    // Clean up local environment
    currentLocals.locals.clear();
    
    return true;
}

void UserFunctionManager::clear() {
    functions.clear();
    currentLocals.locals.clear();
}

std::string UserFunctionManager::normalizeNameForLookup(const std::string& name) const {
    std::string result;
    result.reserve(name.length());
    
    for (char c : name) {
        if (std::isalnum(c)) {
            result += std::toupper(c);
        }
    }
    
    return result;
}

// Conversion helpers
expr::Value UserFunctionManager::toExprValue(const Value& v) {
    switch (v.type) {
        case ScalarType::Int16:
            return expr::Int16{v.i};
        case ScalarType::Single:
            return expr::Single{v.f};
        case ScalarType::Double:
            return expr::Double{v.d};
        case ScalarType::String:
            // For string conversion from StrDesc to std::string
            return expr::Str{std::string(reinterpret_cast<const char*>(v.s.ptr), v.s.len)};
        default:
            throw std::runtime_error("Unknown value type in toExprValue");
    }
}

Value UserFunctionManager::fromExprValue(const expr::Value& v) const {
    return std::visit([this](const auto& val) -> Value {
        using T = std::decay_t<decltype(val)>;
        if constexpr (std::is_same_v<T, expr::Int16>) {
            return Value::makeInt(val.v);
        } else if constexpr (std::is_same_v<T, expr::Single>) {
            return Value::makeSingle(val.v);
        } else if constexpr (std::is_same_v<T, expr::Double>) {
            return Value::makeDouble(val.v);
        } else if constexpr (std::is_same_v<T, expr::Str>) {
            // Convert std::string to StrDesc using StringHeap
            StrDesc sd{};
            if (stringHeap && !val.v.empty()) {
                if (!stringHeap->allocCopy(reinterpret_cast<const uint8_t*>(val.v.data()), 
                                         static_cast<uint16_t>(val.v.size()), sd)) {
                    // Handle allocation failure - create empty string
                    sd.len = 0;
                    sd.ptr = nullptr;
                }
            }
            return Value::makeString(sd);
        } else {
            throw std::runtime_error("Unknown expression value type in fromExprValue");
        }
    }, v);
}

Value UserFunctionManager::convertToType(const Value& value, ScalarType targetType) const {
    switch (targetType) {
        case ScalarType::Int16:
            switch (value.type) {
                case ScalarType::Int16: return value;
                case ScalarType::Single: return Value::makeInt(static_cast<int16_t>(value.f));
                case ScalarType::Double: return Value::makeInt(static_cast<int16_t>(value.d));
                case ScalarType::String: return Value::makeInt(0); // Default for string conversion
            }
            break;
        case ScalarType::Single:
            switch (value.type) {
                case ScalarType::Int16: return Value::makeSingle(static_cast<float>(value.i));
                case ScalarType::Single: return value;
                case ScalarType::Double: return Value::makeSingle(static_cast<float>(value.d));
                case ScalarType::String: return Value::makeSingle(0.0f); // Default for string conversion
            }
            break;
        case ScalarType::Double:
            switch (value.type) {
                case ScalarType::Int16: return Value::makeDouble(static_cast<double>(value.i));
                case ScalarType::Single: return Value::makeDouble(static_cast<double>(value.f));
                case ScalarType::Double: return value;
                case ScalarType::String: return Value::makeDouble(0.0); // Default for string conversion
            }
            break;
        case ScalarType::String:
            switch (value.type) {
                case ScalarType::Int16: {
                    std::string str = std::to_string(value.i);
                    StrDesc sd{};
                    if (stringHeap && !str.empty()) {
                        if (!stringHeap->allocCopy(reinterpret_cast<const uint8_t*>(str.data()), 
                                                 static_cast<uint16_t>(str.size()), sd)) {
                            sd.len = 0;
                            sd.ptr = nullptr;
                        }
                    }
                    return Value::makeString(sd);
                }
                case ScalarType::Single: {
                    std::string str = std::to_string(value.f);
                    StrDesc sd{};
                    if (stringHeap && !str.empty()) {
                        if (!stringHeap->allocCopy(reinterpret_cast<const uint8_t*>(str.data()), 
                                                 static_cast<uint16_t>(str.size()), sd)) {
                            sd.len = 0;
                            sd.ptr = nullptr;
                        }
                    }
                    return Value::makeString(sd);
                }
                case ScalarType::Double: {
                    std::string str = std::to_string(value.d);
                    StrDesc sd{};
                    if (stringHeap && !str.empty()) {
                        if (!stringHeap->allocCopy(reinterpret_cast<const uint8_t*>(str.data()), 
                                                 static_cast<uint16_t>(str.size()), sd)) {
                            sd.len = 0;
                            sd.ptr = nullptr;
                        }
                    }
                    return Value::makeString(sd);
                }
                case ScalarType::String: return value;
            }
            break;
    }
    return value; // Fallback
}

} // namespace gwbasic
