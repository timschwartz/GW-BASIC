# ProgramStore Module

The ProgramStore module implements the program storage functionality for the GW-BASIC reimplementation project. It maintains a linked list of tokenized BASIC program lines, sorted by line number, following the same storage format as the original GW-BASIC interpreter.

## Overview

The ProgramStore class provides a faithful recreation of the GW-BASIC program storage system, including:

- **Linked list storage**: Program lines are stored as a linked list, sorted by line number
- **Compatible format**: Each line follows the GW-BASIC format: `link(2) lineNo(2) tokens... 0x00`
- **Line management**: Insert, replace, delete operations with automatic sorting
- **Memory management**: Efficient storage and retrieval with optional indexing
- **Serialization**: Save/load programs in binary format compatible with GW-BASIC
- **Utilities**: AUTO, RENUM, LIST functionality support

## Key Features

### Storage Format
```
Line Layout: [link(2)] [lineNumber(2)] [tokens...] [0x00]
- link: 2-byte offset to next line (0 for last line)
- lineNumber: 2-byte line number (1-65529)
- tokens: Tokenized BASIC statements
- 0x00: Line terminator
```

### Core Operations
- `insertLine()`: Add or replace a program line
- `deleteLine()`: Remove a program line
- `getLine()`: Retrieve a line by number
- `hasLine()`: Check if a line exists
- `clear()`: Remove all lines (NEW command)

### Iteration and Traversal
- STL-compatible iterators for program traversal
- `findLine()`: Find line by number or next higher
- `getNextLine()`: Get the line following a given line number
- `getLinesInRange()`: Get lines within a range

### Program Analysis
- Line count and total memory usage
- First and last line number tracking
- Program validation and integrity checking
- Debug information generation

### Serialization
- `serialize()`: Convert program to binary format
- `deserialize()`: Load program from binary data
- Compatible with GW-BASIC save/load format

### Line Number Utilities
- `findNextAutoLineNumber()`: Support for AUTO command
- `renumberLines()`: Support for RENUM command
- Line number validation and range checking

## Usage Example

```cpp
#include "ProgramStore.hpp"

// Create a program store
ProgramStore store;

// Add some BASIC lines
std::vector<uint8_t> printTokens = {0x90, 0x22, 'H', 'i', 0x22, 0x00}; // PRINT "Hi"
std::vector<uint8_t> endTokens = {0x80, 0x00}; // END

store.insertLine(10, printTokens);
store.insertLine(20, endTokens);

// Iterate through the program
for (auto it = store.begin(); it != store.end(); ++it) {
    std::cout << "Line " << it->lineNumber << " has " 
              << it->tokens.size() << " tokens\n";
}

// Serialize and save
auto data = store.serialize();
// ... save data to file ...

// Load into new store
ProgramStore newStore;
newStore.deserialize(data);
```

## File Structure

- `ProgramStore.hpp` - Header file with class declaration
- `ProgramStore.cpp` - Implementation file
- `test_programstore.cpp` - Unit tests
- `programstore_example.cpp` - Usage examples
- `Makefile.am` - Build configuration
- `README.md` - This documentation

## Building

The ProgramStore module uses autotools for building:

```bash
# From the ProgramStore directory
make libprogramstore.la    # Build the library
make test_programstore     # Build and run tests
make programstore_example  # Build the example program
make check                 # Run all tests
```

## Memory Management

The ProgramStore uses shared_ptr for automatic memory management:
- Program lines are reference-counted
- No manual memory management required
- Safe iteration and line sharing
- Automatic cleanup on destruction

## Performance Considerations

- **Indexing**: Optional hash-map index for O(1) line lookups
- **Lazy indexing**: Index is rebuilt only when needed
- **Memory efficiency**: Compact storage following GW-BASIC format
- **Iterator performance**: Direct linked-list traversal

## Compatibility

The ProgramStore maintains full compatibility with GW-BASIC:
- Same binary format for serialization
- Identical line number handling (1-65529)
- Compatible token storage format
- Support for all GW-BASIC program management commands

## Thread Safety

The current implementation is **not thread-safe**. If thread safety is required:
- Add mutex protection around modifications
- Use thread-safe shared_ptr operations
- Consider read-write locks for concurrent access

## Future Enhancements

Potential improvements for the ProgramStore:
- Memory-mapped file support for large programs
- Compressed storage for token streams
- Incremental serialization for fast saves
- B-tree indexing for very large programs
- Thread-safe variant with locking

## Error Handling

The ProgramStore provides comprehensive error handling:
- Line number validation (1-65529 range)
- Token termination enforcement
- Structure integrity validation
- Serialization format verification
- Graceful handling of corrupted data

## Testing

The module includes comprehensive tests covering:
- Basic operations (insert, delete, lookup)
- Serialization round-trip testing
- Iterator functionality
- Line number utilities (AUTO, RENUM)
- Validation and error conditions
- Performance characteristics

Run tests with: `make check`

## Integration

The ProgramStore integrates with other GW-BASIC modules:
- **Tokenizer**: Consumes tokenized output for line storage
- **Interpreter**: Provides program execution context
- **Editor**: Supports LIST, AUTO, RENUM commands
- **File I/O**: Handles LOAD, SAVE, MERGE operations
