# Integration Tests

This directory contains integration tests for the GW-BASIC interpreter that test end-to-end functionality.

## Test Programs

- `test_load` - Tests the LOAD functionality by loading a .bas file from disk and executing it
- `test_load.cpp` - Source code for the LOAD functionality test
- `test_save.cpp` - Source code for testing SAVE functionality (creates programs in memory and saves them)
- `test_roundtrip.cpp` - Source code for testing round-trip LOAD/SAVE functionality

## Building and Running Tests

To build the integration tests:

```bash
cd /path/to/GW-BASIC
g++ -std=c++20 -I./src -I./src/InterpreterLoop -I./src/ProgramStore -I./src/Tokenizer -I./src/ExpressionEvaluator -I./src/Runtime tests/integration/test_load.cpp src/InterpreterLoop/.libs/libinterpreterloop.a src/ProgramStore/.libs/libprogramstore.a src/Tokenizer/.libs/libtokenizer.a src/ExpressionEvaluator/.libs/libexpressionevaluator.a -o tests/integration/test_load
```

To run the tests:

```bash
cd /path/to/GW-BASIC
./tests/integration/test_load
```

## Test Coverage

These integration tests verify:

- File I/O operations (LOAD/SAVE)
- FOR-NEXT loop execution
- Program tokenization and detokenization
- Expression evaluation
- Control flow statements
- Round-trip compatibility (save then load)

## Adding New Tests

When adding new integration tests:

1. Create a descriptive filename (e.g., `test_arrays.cpp`)
2. Include error handling and clear output messages
3. Test both success and failure scenarios
4. Document what functionality the test covers
