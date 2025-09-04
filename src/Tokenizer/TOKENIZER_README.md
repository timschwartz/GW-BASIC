# GW-BASIC Tokenizer

A C++ implementation of the GW-BASIC tokenizer (also known as the "cruncher") that converts BASIC source code into the tokenized format used by the original GW-BASIC interpreter.

## Overview

This tokenizer faithfully reproduces the behavior of the original GW-BASIC tokenizer, including:

- **Single-byte tokens** for statements, keywords, and operators (0x80-0xFF range)
- **Two-byte tokens** with prefixes for extended functionality:
  - `0xFE` prefix: Extended statements (FILES, CIRCLE, etc.)
  - `0xFF` prefix: Standard functions (SQR, SIN, etc.)
  - `0xFD` prefix: Extended functions (CVI, CVS, etc.)
- **Encoded constants** for numbers and line numbers
- **Preserved string literals** with proper quote handling
- **Comment processing** including single-quote REM forms

## Features

### Core Functionality
- **Tokenization**: Convert BASIC source code to token bytes
- **Detokenization**: Convert token bytes back to readable BASIC code
- **Reserved word recognition**: Full GW-BASIC keyword set
- **Number encoding**: Integer, single-precision, and double-precision constants
- **String handling**: Quoted strings preserved in tokenized form

### Compatibility
- Supports the complete GW-BASIC statement set (END, FOR, PRINT, etc.)
- Handles extended statements (FILES, CIRCLE, PAINT, etc.)
- Recognizes all standard functions (SQR, SIN, CHR$, etc.)
- Compatible operator tokenization (+, -, *, /, ^, etc.)
- Proper handling of multi-word constructs like "GO TO"

### Advanced Features
- **Alphabetic dispatch**: Efficient keyword lookup using first-letter indexing
- **Type detection**: Automatic recognition of integers vs. floating-point numbers
- **Error handling**: Informative error messages with position information
- **Configurable modes**: Optional extended mode for advanced features

## Usage

### Basic Example

```cpp
#include "tokenizer.h"
#include <iostream>

int main() {
    Tokenizer tokenizer;
    
    // Tokenize a BASIC line
    std::string basicCode = "10 PRINT \"Hello, World!\"";
    auto tokens = tokenizer.tokenize(basicCode);
    
    // Convert to bytes (crunch)
    auto bytes = tokenizer.crunch(basicCode);
    
    // Convert back to text (detokenize)
    std::string restored = tokenizer.detokenize(bytes);
    
    std::cout << "Original: " << basicCode << std::endl;
    std::cout << "Restored: " << restored << std::endl;
    
    return 0;
}
```

### Token Analysis

```cpp
Tokenizer tokenizer;
auto tokens = tokenizer.tokenize("20 FOR I = 1 TO 10");

for (const auto& token : tokens) {
    std::cout << "Type: " << token.type 
              << ", Text: " << token.text 
              << ", Bytes: ";
    for (uint8_t b : token.bytes) {
        std::cout << std::hex << (int)b << " ";
    }
    std::cout << std::endl;
}
```

## Token Types

The tokenizer recognizes these token types:

| Type | Description | Example |
|------|-------------|---------|
| `TOKEN_STATEMENT` | BASIC statements | PRINT, FOR, IF |
| `TOKEN_KEYWORD` | Keywords | THEN, TO, STEP |
| `TOKEN_OPERATOR` | Operators | +, -, *, / |
| `TOKEN_FUNCTION_STD` | Standard functions | SQR, SIN, CHR$ |
| `TOKEN_STATEMENT_EXT` | Extended statements | CIRCLE, FILES |
| `TOKEN_FUNCTION_EXT` | Extended functions | CVI, CVS |
| `TOKEN_NUMBER_INT` | Integer constants | 10, 255 |
| `TOKEN_NUMBER_FLOAT` | Float constants | 3.14, 1.5E10 |
| `TOKEN_NUMBER_DOUBLE` | Double constants | 1.23456789# |
| `TOKEN_STRING_LITERAL` | Quoted strings | "Hello" |
| `TOKEN_LINE_NUMBER` | Line numbers | 10, 100 |
| `TOKEN_IDENTIFIER` | Variables/names | A, X$, COUNT% |
| `TOKEN_REM_COMMENT` | Comments | ' comment |

## Token Encoding

### Single-byte Tokens (0x80-0xFF)

Statement tokens start at 0x80:
- `0x80` END
- `0x81` FOR  
- `0x82` NEXT
- `0x90` PRINT
- etc.

### Two-byte Tokens

Extended functionality uses two-byte tokens:

**Standard Functions (0xFF prefix):**
- `0xFF 0x06` SQR
- `0xFF 0x08` SIN
- `0xFF 0x15` CHR$

**Extended Statements (0xFE prefix):**
- `0xFE 0x00` FILES
- `0xFE 0x10` CIRCLE
- `0xFE 0x0E` PAINT

**Extended Functions (0xFD prefix):**
- `0xFD 0x00` CVI
- `0xFD 0x01` CVS

### Number Encoding

Numbers are encoded with type-specific prefixes:
- **Line numbers:** `0x0D` + 16-bit little-endian value
- **Integers:** `0x11` + 16-bit little-endian value  
- **Floats:** `0x1D` + 32-bit IEEE 754 representation
- **Doubles:** `0x1F` + 64-bit IEEE 754 representation

## Building

Use the provided Makefile:

```bash
make clean && make
make test    # Run the test program
```

Or compile manually:

```bash
g++ -std=c++11 -Wall -Wextra -O2 tokenizer.cpp test_tokenizer.cpp -o test_tokenizer
```

## Testing

The included test program demonstrates tokenization of various BASIC constructs:

```bash
./test_tokenizer
```

This will tokenize and detokenize several example BASIC lines, showing the internal byte representation.

## Architecture

The tokenizer uses several key components:

- **Reserved word tables**: Hash maps for O(1) keyword lookup
- **Alphabetic dispatch**: 26 tables (A-Z) for efficient first-letter matching
- **Token encoding**: Type-specific byte sequence generation
- **Lexical scanner**: Character-by-character source analysis
- **Error handling**: Position-aware error reporting

## Compatibility Notes

This implementation aims for behavioral compatibility with the original GW-BASIC tokenizer:

- Token values match the original `IBMRES.ASM` assignments
- Multi-word constructs like "GO TO" are properly recognized
- Single-quote comments are tokenized as REM statements
- String literals preserve their quote delimiters
- Number encoding follows the original format specifications

## Limitations

- Does not implement the complete GW-BASIC runtime environment
- Focuses on tokenization/detokenization rather than execution
- Some advanced tokenizer features may not be fully implemented
- Error handling is simplified compared to the original

## License

This implementation is provided for educational and research purposes, following the principles established by the original GW-BASIC open source release.

## References

- Original GW-BASIC source code (Microsoft, 2020)
- `asm/IBMRES.ASM` - Reserved word tables and token definitions
- `docs/appendix_tokens.md` - Token specification reference
- `docs/architecture_map.md` - System architecture overview
