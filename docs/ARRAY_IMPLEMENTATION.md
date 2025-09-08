# Array Element Access Implementation

## Overview

This implementation adds array element access functionality to the GW-BASIC expression evaluator. It supports the traditional GW-BASIC syntax for array subscripts using both parentheses `()` and square brackets `[]`.

## Features

### Syntax Support
- **Parentheses**: `A(1)`, `B(2,3)`, `MATRIX(I,J)`
- **Square Brackets**: `A[1]`, `B[2,3]`, `MATRIX[I,J]`
- **Mixed expressions**: `A(I+1)`, `B(2*J,K-1)`

### Multi-dimensional Arrays
- Supports arrays of any dimensionality
- Subscripts are separated by commas: `ARRAY(1,2,3)`
- Each subscript can be any valid expression

### Type Support
- Integer arrays: `A(1)` returns `Int16`
- String arrays: `NAMES$(1)` returns `Str`
- Other numeric types: `SINGLES(1)` returns `Single`, `DOUBLES(1)` returns `Double`

### Function vs Array Disambiguation
The evaluator intelligently distinguishes between function calls and array access:

- **Known Functions**: `SIN(X)`, `LEN(S$)`, `CHR$(65)` are always treated as function calls
- **Unknown Identifiers**: `A(1)`, `DATA(5)` are first tried as array access, then as function calls if array doesn't exist

## Integration

### Environment Setup
To use array access, set up the `getArrayElem` callback in the `Env` structure:

```cpp
env.getArrayElem = [](const std::string& name, const std::vector<Value>& indices, Value& out) -> bool {
    // Your array lookup logic here
    // Return true if array element found, false otherwise
};
```

### Example Usage

```cpp
#include "ExpressionEvaluator.hpp"
using namespace expr;

ExpressionEvaluator ev(nullptr);
Env env;

// Set up array data
std::unordered_map<std::string, std::vector<int16_t>> arrays;
arrays["A"] = {10, 20, 30};

env.getArrayElem = [&arrays](const std::string& name, const std::vector<Value>& indices, Value& out) -> bool {
    auto it = arrays.find(name);
    if (it == arrays.end()) return false;
    
    if (indices.size() != 1 || !std::holds_alternative<Int16>(indices[0])) return false;
    int16_t index = std::get<Int16>(indices[0]).v;
    
    if (index < 0 || index >= static_cast<int16_t>(it->second.size())) return false;
    
    out = Int16{it->second[index]};
    return true;
};

// Use array access in expressions
auto result = ev.evaluate(asciiExpr("A(1) + A(2)"), 0, env);
```

## Implementation Details

### New Methods
- `isKnownFunction(const std::string& name)` - Checks if an identifier is a known built-in function
- `parseArrayAccess(const std::string& arrayName, ...)` - Parses array subscripts and resolves element access

### Modified Methods
- `parsePrimary()` - Enhanced to handle array access vs function call disambiguation
- `parseArgumentList()` - Extended to support both `()` and `[]` syntax

### Error Handling
- **Undefined Arrays**: Throws `BasicError` with "Undefined array" message
- **Invalid Subscripts**: Errors bubble up from the array resolver callback
- **Syntax Errors**: Handles missing brackets, invalid subscript expressions, etc.

## Testing

The implementation includes comprehensive tests covering:
- Basic 1D array access: `A(0)`, `B[1]`
- Multi-dimensional arrays: `MATRIX(2,1)`
- String arrays: `NAMES$(0)`
- Complex expressions: `A(I+1)`, `A(0) + B(1)`
- Function disambiguation: `LEN("TEST")` vs `A(1)`
- Error cases: undefined arrays, invalid indices

## Compatibility

This implementation maintains full compatibility with existing GW-BASIC expression evaluation while adding the new array access functionality. All existing tests continue to pass.
