# GW-BASIC Reimplementation in C++

A modern C++ reimplementation of Microsoft GW-BASIC, providing a compatible interpreter for classic BASIC programs.

## Features

- **Tokenizer**: Complete tokenization compatible with original GW-BASIC
- **Program Storage**: Linked-list based program storage matching original format  
- **Expression Evaluator**: Mathematical and logical expression evaluation
- **Control Flow**: Support for FOR-NEXT loops, IF-THEN, GOTO statements
- **File I/O**: LOAD and SAVE commands for program persistence
- **Interactive Shell**: SDL-based graphical interface

## Building

This project uses GNU Autotools for building:

```bash
# Generate configure script (if building from git)
autoreconf -fi

# Configure the build
./configure

# Build the project
make

# Run tests
make check
```

## Running

After building, run the interpreter:

```bash
./src/gw-basic
```

## Project Structure

```
├── src/                     # Source code
│   ├── Tokenizer/          # BASIC tokenization/detokenization
│   ├── ProgramStore/       # Program line storage and management  
│   ├── InterpreterLoop/    # Execution engine and statement dispatcher
│   ├── ExpressionEvaluator/# Mathematical expression evaluation
│   ├── NumericEngine/      # Numeric computation engine
│   ├── Runtime/            # Variable storage and runtime stack
│   └── main.cpp           # SDL-based GUI application
├── examples/               # Example BASIC programs
│   ├── test.bas           # Simple test program with FOR loop
│   ├── test_for_loop.bas  # Comprehensive FOR-NEXT tests
│   └── sample.bas         # Original sample program
├── tests/                  # Test suite
│   ├── integration/       # Integration tests
│   └── [unit tests]       # Unit tests for individual components
└── docs/                  # Documentation
```

## Testing

### Unit Tests
```bash
make check
```

### Integration Tests
```bash
make -C tests/integration check-integration
```

## Supported BASIC Features

### Statements
- `PRINT` - Output text and expressions
- `LET` / Implied assignment - Variable assignment
- `FOR...NEXT` - Counting loops with optional STEP
- `IF...THEN` - Conditional execution
- `GOTO` - Unconditional jump
- `END`/`STOP` - Program termination
- `LOAD` - Load program from disk
- `SAVE` - Save program to disk

### Data Types
- Integer (16-bit)
- Single precision floating point
- Double precision floating point  
- String

### Expressions
- Arithmetic operators: `+`, `-`, `*`, `/`, `^`
- Comparison operators: `=`, `<`, `>`, `<=`, `>=`, `<>`
- Logical operators: `AND`, `OR`, `XOR`, `NOT`

## Examples

Load and run a program:
```basic
LOAD "examples/test.bas"
RUN
```

Interactive programming:
```basic
10 FOR I = 1 TO 10
20 PRINT "Hello World"; I  
30 NEXT I
RUN
```

## Development

The codebase is organized into modular components that can be developed and tested independently. Each major component has its own unit tests and example programs.

## License

[Add license information here]

