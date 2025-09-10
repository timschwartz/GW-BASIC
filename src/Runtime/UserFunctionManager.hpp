#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <variant>

#include "Value.hpp"
#include "StringHeap.hpp"
#include "../ExpressionEvaluator/ExpressionEvaluator.hpp"

// Forward declarations
class Tokenizer;

namespace gwbasic {

struct UserFunction {
    std::string name;                    // Function name (e.g., "FNAREA")
    std::vector<std::string> parameters; // Parameter names (e.g., ["R"])
    std::vector<uint8_t> expression;     // Tokenized expression body
    ScalarType returnType;               // Expected return type
    
    UserFunction() = default;
    UserFunction(const std::string& n, const std::vector<std::string>& params, 
                 const std::vector<uint8_t>& expr, ScalarType type)
        : name(n), parameters(params), expression(expr), returnType(type) {}
};

class UserFunctionManager {
public:
    explicit UserFunctionManager(StringHeap* heap = nullptr, std::shared_ptr<Tokenizer> tokenizer = nullptr);
    
    // Function definition management
    bool defineFunction(const std::string& name, 
                       const std::vector<std::string>& parameters,
                       const std::vector<uint8_t>& expression,
                       ScalarType returnType);
    
    bool isUserFunction(const std::string& name) const;
    
    // Function call execution
    bool callFunction(const std::string& name, 
                     const std::vector<Value>& arguments,
                     Value& result) const;
    
    // Utility methods
    void clear();
    size_t getFunctionCount() const { return functions.size(); }
    
    // Get function info for debugging
    const UserFunction* getFunction(const std::string& name) const;
    
private:
    std::unordered_map<std::string, UserFunction> functions;
    StringHeap* stringHeap;
    std::shared_ptr<Tokenizer> tokenizer;
    
    // Helper methods
    std::string normalizeNameForLookup(const std::string& name) const;
    ScalarType inferReturnTypeFromName(const std::string& name) const;
    
    // Value conversion helpers
    static expr::Value toExprValue(const Value& v);
    Value fromExprValue(const expr::Value& v) const;
    Value convertToType(const Value& value, ScalarType targetType) const;
    
    // Parameter binding and local variable management
    struct LocalEnvironment {
        std::unordered_map<std::string, Value> locals;
    };
    
    mutable LocalEnvironment currentLocals; // Thread-local storage for function execution
};

} // namespace gwbasic
