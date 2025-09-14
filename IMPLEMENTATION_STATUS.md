# GW-BASIC Implementation Status

A comprehensive status overview of the GW-BASIC reimplementation in C++.

**Last Updated:** September 14, 2025  
**Current Version:** 0.1

## Summary

This project includes:
 - ✅ **SAVE**: Basic file I/O for program storage
- ✅ **SCREEN Statement**: Complete implementation with 14 video modes (0-13), dynamic window resizing, graphics mode switching, and text framebuffer overlay for graphics modes
- ✅ **COLOR Statement**: Complete foreground and background color support with standard 16-color CGA/EGA/VGA palette (colors 0-15 for foreground, 0-7 for background)
- ✅ **WIDTH Statement**: Text column width control (supports 40/80/132); updates text mode layout and resizes SDL window appropriately in GUI mode; safe no-op in console mode
 - ✅ **LOCATE Statement**: Sets text cursor row/column and visibility in GUI mode; accepted as a no-op in console mode; tolerant parsing of omitted/null arguments; start/stop (cursor shape) parsed but currently ignored
- ✅ **CLS Statement**: Complete screen clearing implementation working in both GUI mode (clearScreen() callback) and console mode (ANSI escape sequences), integrated with colon-separated statement chains and immediate mode execution
- ✅ **Sequential File I/O**: Complete OPEN, CLOSE, PRINT#, INPUT# implementation with file mode support (INPUT/OUTPUT/APPEND), file number management, and proper tokenizer integration
- ✅ **Random Access File Support**: OPEN with LEN for custom record lengths, plus FIELD/LSET/RSET mapping and GET/PUT record I/O (file-side complete)
- ✅ **FILES Command**: Implemented extended statement FILES with DOS-style wildcard matching (* and ?) and optional path prefix; outputs matching names via print callback
- ✅ **KILL Command**: Implemented extended statement KILL for file deletion with proper error handling for non-existent files and directories
- ✅ **NAME Command**: Implemented extended statement NAME for file renaming with 'oldname AS newname' syntax and comprehensive error handling
 - ✅ **Error Handling**: Complete ON ERROR GOTO, RESUME, ERROR statement implementation with proper error routing, ERL/ERR variables, and comprehensive error handler managementThis is a modern C++ reimplementation of Microsoft GW-BASIC, designed to be compatible with the original interpreter while using modern programming practices and tools. The implementation is **approximately 95% complete** with core functionality operational, robust string memory management implemented, complete array runtime support, comprehensive event trap handling, full function key support, complete SCREEN statement with graphics mode support and text framebuffer, user-defined function support (DEF FN), complete INPUT/PRINT behavior aligned with GW-BASIC semantics, fixed OPEN statement LEN parameter support, and comprehensive error handling system. Recent work added comprehensive function key support (F1-F10) with soft key expansion and event trap integration, unified console and GUI execution through the InterpreterLoop, added extended-statement handling (including SYSTEM), fixed PRINT/PRINT USING parsing at line terminators, implemented full SCREEN statement functionality with 14 video modes, dynamic window resizing, and scaled text overlay for graphics modes, completed DEF FN user-defined function implementation with proper error handling and line number reporting, and finalized the complete error handling system with ON ERROR GOTO, RESUME, and ERROR statement implementations.
 - ✅ NEW: Memory peek/poke and type defaults
   - DEF SEG [= value] implemented; maintains current segment for memory operations
   - PEEK(address) wired to read from simulated DOS memory at segment*16 + address
   - POKE address, value writes a byte to simulated DOS memory using current DEF SEG
   - DEFINT/DEFSNG/DEFDBL/DEFSTR implemented to set default types by letter ranges

## ✅ Completed Components

### Core Infrastructure (95% Complete)
- ✅ **Build System**: Complete autotools-based build system with `make`, `make check`, `make install`
- ✅ **Project Structure**: Modular design with separate components (Tokenizer, ProgramStore, etc.)
- ✅ **Documentation**: Architecture documentation, implementation guides, and module READMEs
- ✅ **Testing Framework**: Unit tests for all major components, integration tests for key features

### Tokenizer Component
**Status: ✅ COMPLETE (100%)**
- ✅ Basic tokenization (keywords, operators, numbers, strings)
- ✅ Reserved word recognition and statement/keyword mapping
- ✅ Operator tokenization with proper precedence
- ✅ String literal handling with escape sequences
- ✅ Numeric literal parsing (integers, floats, doubles)
- ✅ Multi-character operators (<=, >=, <>)
- ✅ Hexadecimal number support (&HFF format)
- ✅ Octal number support (&377 format)
- ✅ Number type suffixes (%, !, #)
- ✅ Line ending handling (LF, CRLF, CR)
- ✅ Line continuation with underscore (_ + line ending)
- ✅ Underscore support in identifiers (MY_VARIABLE)
- ✅ Comprehensive error handling
- ✅ Full test coverage (85 assertions, 9 test cases)
- ✅ **File I/O Token Support**: '#' operator and 'AS' keyword tokenization with proper detokenizer spacing rules

**Files**: `src/Tokenizer/` (Tokenizer.hpp, Tokenizer.cpp, test_tokenizer.cpp)

**Recent Enhancements:**
- Line continuation preprocessing with underscore (`_`) support
- Enhanced identifier parsing to include underscores in variable names
- Comprehensive test coverage for line continuation edge cases
- Cross-platform line ending compatibility (LF, CRLF, CR)
- Recognizes extended-statement prefix (0xFE) and maps extended statements (e.g., SYSTEM)
- **NEW**: Added '#' operator tokenization for file I/O statements (PRINT#, INPUT#)
- **NEW**: Added 'AS' keyword tokenization for OPEN statements
- **NEW**: Detokenizer spacing rules updated to prevent extra spaces before commas and after '#' operator
- **FIXED**: Enhanced LEN token handling to prevent incorrect tokenization when followed by = (e.g., LEN = 64)
- **FIXED**: Improved detokenizer spacing for proper display of OPEN...LEN statements

### Program Store (95% Complete)
- ✅ **Linked List Storage**: Compatible with original GW-BASIC format
- ✅ **Line Management**: Insert, replace, delete operations with automatic sorting
- ✅ **Serialization**: Save/load programs in binary format
- ✅ **Memory Management**: Efficient storage with optional indexing
- ✅ **Iterator Support**: STL-compatible iteration through program lines
- ✅ **Line Utilities**: Support for AUTO, RENUM functionality
- ✅ **Validation**: Program integrity checking and error handling

**Files**: `src/ProgramStore/` (ProgramStore.hpp, ProgramStore.cpp, test_programstore.cpp)

### Expression Evaluator (98% Complete)
- ✅ **Pratt Parser**: Precedence-based expression parsing
- ✅ **Numeric Types**: Int16, Single, Double with proper coercion
- ✅ **String Support**: String expressions and operations
- ✅ **Arithmetic Operators**: +, -, *, /, ^, MOD
- ✅ **Comparison Operators**: =, <>, <, >, <=, >=
- ✅ **Logical Operators**: AND, OR, XOR, NOT, EQV, IMP
- ✅ **Variable References**: Integration with variable table
- ✅ **Tokenized Constants**: Decoding of embedded numeric constants
- ✅ **Built-in Functions**: Complete implementation of math and string functions
  - Math: ABS, SGN, INT, SQR, SIN, COS, TAN, ATN, LOG, EXP, RND
  - String: LEN, ASC, CHR$, LEFT$, RIGHT$, MID$, STR$, VAL
  - Conversion: CINT, CSNG, CDBL
- ✅ **Function Call Parsing**: Support for nested function calls and argument lists
- ✅ **User-Defined Functions**: Complete FN function call parsing and evaluation with UserFunctionManager integration for DEF FN statement support
- ✅ **ASCII and Tokenized Parsing**: Handles both ASCII text and tokenized function calls
- ✅ **Floating-point Support**: Complete support for floating-point numbers in function arguments
- ✅ **Array Element Access**: Complete support for array subscript syntax (A(I,J) and A[I,J])
- ✅ **Function vs Array Disambiguation**: Intelligent parsing to distinguish function calls from array access

**Files**: `src/ExpressionEvaluator/` (ExpressionEvaluator.hpp, ExpressionEvaluator.cpp, test_expression.cpp)  
**Documentation**: `docs/ARRAY_IMPLEMENTATION.md` - Complete implementation guide for array element access

**Recent Enhancements:**
- Added comprehensive built-in function support with 20+ functions
- Implemented function call parsing framework with argument list handling
- Added support for both ASCII and tokenized function recognition
- Enhanced floating-point number parsing in ASCII literals
- Case-insensitive matching for built-in function names to avoid "Unknown function" due to case
- Argument list parsing supports tokenized delimiters used in tokenized programs: '(' = 0xF4, ')' = 0xF5, ',' = 0xF6
- **NEW**: Complete array element access implementation with A(I,J) and A[I,J] syntax support
- **NEW**: Intelligent function vs array disambiguation using built-in function registry
- **NEW**: Multi-dimensional array support with proper index expression evaluation
- **NEW**: Complete user-defined function support with FN function call parsing and evaluation
- **NEW**: Enhanced token recognition for proper semicolon (0xF7) and comma (0xF6) handling in expressions
- Complete test coverage for all implemented functions, array operations, and user-defined functions

### Microsoft Binary Format (MBF) Compatibility
**Status: ✅ COMPLETE (100%)**
- ✅ **MBF32 Format**: Single-precision Microsoft Binary Format with bias 128 exponent
- ✅ **MBF64 Format**: Double-precision Microsoft Binary Format with bias 128 exponent  
- ✅ **IEEE↔MBF Conversion**: Accurate conversion between IEEE 754 and MBF formats
- ✅ **Round-to-Nearest-Even**: Proper rounding semantics for conversion accuracy
- ✅ **Normalization Functions**: Handle zero, infinity, and denormalized number cases
- ✅ **NumericEngine Integration**: Seamless integration with existing numeric operations
- ✅ **Comprehensive Testing**: 88 test assertions covering all conversion scenarios

**Files**: `src/NumericEngine/` (MBFFormat.hpp, MBFFormat.cpp, test_mbf.cpp)

**Technical Implementation:**
- Bit-for-bit compatibility with original GW-BASIC numeric operations
- Supports both single-precision (MBF32) and double-precision (MBF64) formats
- Proper handling of exponent bias differences (IEEE 127/1023 vs MBF 128)
- Edge case handling for zero, infinity, and subnormal numbers
- Integration with NumericEngine for transparent MBF-aware operations

### Numeric Engine (100% Complete)
- ✅ **Basic Arithmetic**: Add, subtract, multiply, divide, modulo with proper overflow handling
- ✅ **Comparison Operations**: All comparison operators with GW-BASIC semantics
- ✅ **Math Functions**: Complete implementation - ABS, SGN, INT, FIX, SQR, LOG, EXP, SIN, COS, TAN, ATN
- ✅ **Random Numbers**: RND function with seed support and RANDOMIZE
- ✅ **Type Conversions**: Between Int16, Single, Double with GW-BASIC rules
- ✅ **Unary Operations**: Negate, absolute value, sign with proper overflow handling
- ✅ **Error Handling**: Proper GW-BASIC error codes (6, 11, 5, 13)
- ✅ **PRINT USING Formatting**: Complete implementation with numeric patterns (#, ., ,), currency ($), sign (+/-), and asterisk fill (*)
- ✅ **MBF Compatibility Layer**: Complete Microsoft Binary Format support for bit-for-bit compatibility with original GW-BASIC numeric operations
- ✅ **Utility Functions**: isZero, isNegative, isInteger for type checking and validation

**Files**: `src/NumericEngine/` (NumericEngine.hpp, NumericEngine.cpp, test_numeric.cpp, MBFFormat.hpp, MBFFormat.cpp, test_mbf.cpp)

### String Heap with Garbage Collection
**Status: ✅ COMPLETE (100%)**
- ✅ **Automatic Garbage Collection**: Mark-compact GC with configurable policies (OnDemand, Aggressive, Conservative)
- ✅ **StringRootProvider Pattern**: Integration system allowing components to register as GC root sources
- ✅ **Memory Management**: Downward-growing heap architecture with proper bounds checking
- ✅ **String Protection**: RAII-based temporary string protection during complex operations
- ✅ **Statistics Tracking**: Comprehensive memory usage and GC performance metrics
- ✅ **StringManager Interface**: High-level string operations (creation, concatenation, slicing, search, comparison)
- ✅ **Temporary String Pool**: RAII-managed temporary strings with automatic cleanup and vector pre-allocation
- ✅ **VariableTable Integration**: Seamless integration with variable storage as GC root provider
- ✅ **Comprehensive Testing**: 225 test assertions covering all GC scenarios, memory management, and string operations

**Technical Features:**
- Mark-compact garbage collection with address-sorted compaction to prevent corruption
- Configurable GC policies with automatic triggering based on memory pressure or fragmentation
- StringRootProvider pattern for seamless integration with VariableTable and other components
- RAII-based string protection (StringProtector) for safe temporary string management
- Pre-allocated vector capacity to prevent pointer invalidation in temporary string pool
- Comprehensive statistics including allocation counts, GC cycles, bytes reclaimed, and fragmentation metrics

**Files**: `src/Runtime/StringHeap.hpp`, `src/Runtime/StringManager.hpp`, `src/Runtime/StringTypes.hpp`, `tests/test_string_heap.cpp`, `tests/test_string_manager.cpp`

### Runtime System (100% Complete)
- ✅ **Variable Table**: DEFTBL-driven default typing, suffix handling with StringRootProvider integration
- ✅ **Runtime Stack**: FOR/NEXT and GOSUB/RETURN frame management with error handling support
- ✅ **String Types**: String descriptor system with length/pointer and temporary pool management
- ✅ **Value System**: Unified value type supporting all GW-BASIC data types
- ✅ **Memory Management**: Reference counting and basic cleanup
- ✅ **Array Infrastructure**: Array headers and multi-dimensional support (ArrayTypes.hpp)
- ✅ **String Heap with Garbage Collection**: Complete automatic memory management with mark-compact GC, configurable policies (OnDemand/Aggressive/Conservative), StringRootProvider pattern, string protection mechanism, and comprehensive statistics
- ✅ **StringManager**: High-level interface for string operations including creation, concatenation, slicing (LEFT$, RIGHT$, MID$), search (INSTR), comparison, and RAII-managed temporary strings
- ✅ **Array Runtime**: Complete array implementation with ArrayManager, DIM statement support, multi-dimensional arrays, and full integration with expression evaluator and variable table
- ✅ **UserFunctionManager**: Complete user-defined function management with function definition storage, parameter binding, and expression evaluation within function scope
- ✅ **Event Trap System**: Complete EventTrapSystem with support for KEY, ERROR, TIMER, PEN, PLAY, STRIG, and COM events with trap configuration, event injection, and checking mechanisms
- ✅ **Data Management**: DataManager component for READ/DATA/RESTORE functionality with sequential data reading, type conversion, and pointer management
- ✅ **File Management**: FileManager component for sequential file I/O operations with file number management, mode validation (INPUT/OUTPUT/APPEND), and read/write operations
- ✅ **String Function Integration**: StringFunctionProcessor providing unified interface between ExpressionEvaluator and StringManager with proper memory management, type conversion (expr::Value ↔ gwbasic::Value), and comprehensive string function support (CHR$, STR$, VAL, LEFT$, RIGHT$, MID$, LEN, ASC, INSTR)

**Files**: `src/Runtime/` (Value.hpp, VariableTable.hpp, RuntimeStack.hpp, StringTypes.hpp, ArrayTypes.hpp, ArrayManager.hpp, ArrayManager.cpp, StringHeap.hpp, StringManager.hpp, EventTraps.hpp, EventTraps.cpp, DataManager.hpp, DataManager.cpp, FileManager.hpp, FileManager.cpp, UserFunctionManager.hpp, UserFunctionManager.cpp, StringFunctions.hpp, StringFunctions.cpp)

**Recent Enhancements:**
- ✅ **Complete Array Runtime**: Full ArrayManager implementation with create/access/modify operations for multi-dimensional arrays
- ✅ **DIM Statement Support**: Complete parsing and execution of DIM statements with tokenized parentheses support (0xf3/0xf4)
- ✅ **Array-Variable Integration**: Seamless integration between ArrayManager and VariableTable with StringRootProvider GC support
- ✅ **Multi-dimensional Arrays**: Support for arrays up to multiple dimensions with proper bounds checking and memory management
- ✅ **Array Element Access**: Full support for both A(I,J) and A[I,J] syntax in expressions with type coercion
- ✅ **Comprehensive Testing**: All array operations validated through comprehensive test suite (test_array_manager)
- ✅ **Sequential File I/O**: Complete FileManager implementation with OPEN, CLOSE, PRINT#, INPUT# support, file mode validation, and proper error handling

### Interpreter Loop (90% Complete)
- ✅ **Execution Engine**: Step-by-step program execution
- ✅ **Statement Dispatch**: Pluggable statement handler system
- ✅ **Control Flow**: Program counter management, jumping
- ✅ **Trace Support**: Debug tracing with callback system
- ✅ **Immediate Mode**: Direct statement execution outside programs
- ✅ **Error Handling**: Exception propagation and recovery
- ✅ **Event Trap Integration**: Event checking during execution loop for KEY, ERROR, and TIMER traps
- ✅ **Key Event Injection**: Callback system for handling keyboard events in traps

**Files**: `src/InterpreterLoop/` (InterpreterLoop.hpp, InterpreterLoop.cpp, test_interpreterloop.cpp)
**Recent Enhancements:**
- Proper propagation of the 0xFFFF halt sentinel for END/STOP/SYSTEM to cleanly terminate execution without spurious syntax errors
- Unified console and GUI paths to always run through InterpreterLoop (console no longer bypasses the loop)

### Basic Dispatcher (100% Complete)
- ✅ **PRINT Statement**: Basic text output with separators (`;`, `,`)
- ✅ **PRINT USING Statement**: Formatted numeric output with format patterns (###.##, comma separators, currency symbols, sign indicators, asterisk fill)
- ✅ **LET/Assignment**: Variable assignment with type coercion
- ✅ **Array Assignment**: Complete array element assignment with A(I,J) = expression syntax
- ✅ **DIM Statement**: Complete implementation with tokenized parentheses support for declaring arrays
- ✅ **IF-THEN-ELSE**: Conditional execution with inline and line number branches
- ✅ **FOR-NEXT Loops**: Complete loop implementation with STEP support
- ✅ **GOTO/GOSUB/RETURN**: Jump statements with proper stack management
- ✅ **ON GOTO/GOSUB**: Computed jumps with line number lists
- ✅ **Event Trap Statements**: Complete ON KEY, ON ERROR, ON TIMER statement handling
- ✅ **Trap Control Statements**: KEY ON/OFF, ERROR handling (doERROR, doRESUME, doKEY, doTIMER)
- ✅ **INPUT Statement**: Complete user input implementation with prompt support, cross-platform input handling (console/GUI modes), type coercion, and test mode support
- ✅ **READ/DATA/RESTORE**: Complete data statement implementation with DataManager integration for sequential data reading and restoration
- ✅ **DEF Statement**: Complete user-defined function implementation with DEF FN parsing, parameter binding, and function call evaluation
- ✅ **END/STOP**: Program termination
- ✅ **SYSTEM**: Implemented as a program statement via extended token 0xFE 0x02, halting execution
- ✅ **LOAD/SAVE**: Basic file I/O for program storage
- ✅ **SCREEN Statement**: Complete implementation with 14 video modes (0-13), dynamic window resizing, graphics mode switching, and text framebuffer overlay for graphics modes
- ✅ **Sequential File I/O**: Complete OPEN, CLOSE, PRINT#, INPUT# implementation with file mode support (INPUT/OUTPUT/APPEND), file number validation, and proper tokenizer integration for '#' operator and 'AS' keyword
- ✅ **Random Access File Support**: OPEN with LEN for custom record lengths, plus FIELD/LSET/RSET and GET/PUT record I/O
- ✅ **Graphics Drawing**: Complete PSET, LINE, CIRCLE implementation with coordinate parsing, color parameter support, proper tokenization handling for parentheses (244), commas (246), and minus operators (232), and integration with GraphicsContext API
- ✅ **Error Handling Enhancement**: Improved error reporting with proper line number context for syntax errors, ensuring all expression evaluator exceptions are caught and re-thrown with line information
- ✅ **OPEN Statement LEN Parameter**: Fixed tokenization and parsing issues with LEN parameter syntax, supporting multiple = token formats (0xD1, 0xD2, 0xE5) for robust parsing
- ✅ **Complete Error Handling**: ON ERROR GOTO and RESUME statement implementation with proper error routing, ERL/ERR variables, ERROR statement simulation, and comprehensive error handler management
 - ✅ NEW: **DEF SEG / PEEK / POKE**: Added simple memory model (1 MiB) with current segment tracking; PEEK and POKE operate on current segment offset, DEF SEG with or without = supported
 - ✅ NEW: **DEFINT/DEFSNG/DEFDBL/DEFSTR**: Letter-range parsing (e.g., A-C, M, X-Z) updates DefaultTypeTable for implicit variable typing
 - ✅ NEW: **WIDTH Statement**: Parses WIDTH n; validates allowed columns (40, 80, 132); invokes UI width callback; throws "Illegal function call" on invalid values
  - ✅ NEW: **LOCATE Statement (parsing + behavior)**: Robust, tokenizer-aware parsing for LOCATE [row][,[column][,[cursor][,[start][,[stop]]]]] in immediate/program modes; accepts omitted/null arguments and consumes trailing tokens to avoid spurious syntax errors. Wired via dispatcher callback: in GUI mode updates cursor position (1-based to internal 0-based) and cursor visibility; in console mode it's a safe no-op. Start/stop (cursor shape) parsed but ignored for now.
  - ✅ NEW: **CLS Statement**: Added complete screen clearing implementation with doCLS() method and ClsCallback type; works correctly in both GUI mode (clearScreen() callback) and console mode (ANSI escape sequences); properly integrated with colon-separated statement chains and immediate mode execution; no parameters parsed (CLS takes no arguments in GW-BASIC)

**Files**: `src/InterpreterLoop/BasicDispatcher.hpp`, `src/Graphics/` (GraphicsContext.hpp, GraphicsContext.cpp)
**Recent Enhancements:**
- Added handling for extended-statement prefix 0xFE (e.g., SYSTEM, TIMER)
- Fixed PRINT/PRINT USING to stop parsing at 0x00 end-of-line terminator
- Corrected newline and separator semantics: trailing semicolon suppresses newline; commas advance to tab stops
- Removed duplicate direct stdout output; output and prompts now go through printCallback only
- **NEW**: Complete sequential file I/O implementation with doOPEN, doCLOSE, PRINT#, INPUT# supporting all file modes (INPUT/OUTPUT/APPEND)
- **NEW**: FileManager integration with file number validation (1-255) and proper error mapping by file mode
- **NEW**: Tokenizer support for '#' operator and 'AS' keyword with mixed ASCII/tokenized parsing support
- **FIXED**: OPEN statement LEN parameter parsing with support for multiple = token formats and flexible LEN keyword recognition
- **NEW**: Complete DEF FN implementation with doDEF statement parsing, UserFunctionManager integration, and FN function call support
- **NEW**: Enhanced error handling with proper line number reporting for all syntax errors, including expression evaluator exceptions
- **NEW**: Complete graphics drawing implementation with doPSET, doLINE, doCIRCLE supporting coordinate parsing, color parameters, and proper token handling for parentheses (244), commas (246), closing parentheses (245), and minus operators (232)
- **NEW**: Implemented FILES extended statement with path splitting, case-insensitive wildcard matching, and stable sorted output
- **NEW**: Implemented KILL extended statement for file deletion with filesystem error handling and proper GW-BASIC error codes
- **NEW**: Implemented NAME extended statement for file renaming with AS keyword parsing and comprehensive error handling for file conflicts
 - **NEW**: Improved COLOR statement parsing: tokenizer-aware comma handling, optional border argument parsed (ignored for compatibility), and trailing token consumption to avoid spurious syntax errors in immediate mode
 - **NEW**: Added WIDTH statement handling with callback wiring and tokenization-aware parsing

### User Interface (90% Complete)
- ✅ **SDL3 Integration**: Modern graphics framework for cross-platform support
- ✅ **Text Mode Emulation**: 80x25 character display with CGA-style colors
- ✅ **Graphics Mode Support**: Complete SCREEN statement implementation with 14 video modes, dynamic window resizing (320x200 to 720x400), pixel buffer management, and scaled text overlay
- ✅ **Text Framebuffer**: Automatic character scaling system for text display in graphics modes with proper bounds checking and background support
- ✅ **Keyboard Input**: Full keyboard handling with command history and event trap integration
- ✅ **Function Key Support**: Complete F1-F10 function key implementation with soft key expansion and event trap integration
- ✅ **Soft Key System**: Default soft key assignments (F1="LIST", F2="RUN\r", F3="LOAD\"", etc.) with expandable execution
- ✅ **Event Trap Integration**: Function keys properly check for active KEY traps before expanding soft keys
- ✅ **Interactive Shell**: Immediate mode and program entry
- ✅ **Basic Commands**: LIST, RUN, NEW, CLEAR, SYSTEM
- ✅ **Program Editing**: Line number-based program entry and editing
- ✅ **Error Display**: Error messages and runtime feedback
- ✅ **Command Line Loading**: Automatic file loading from command line arguments with --help support
- ✅ **File Loading Integration**: Complete loadFile() method with error handling and user feedback
- ✅ **Console Path Unification**: Console execution uses InterpreterLoop; immediate SYSTEM handled pre-tokenization
- ⚠️ **Missing**: Advanced editing features
- ⚠️ **Missing**: Screen positioning, cursor control

**Files**: `src/main.cpp`, `src/BitmapFont.hpp`

**Recent Enhancements:**
- Complete SCREEN statement implementation with 14 video modes (0-13) supporting dynamic window resizing from 320x200 to 720x400
- Advanced text framebuffer system with automatic character scaling for graphics modes (0.5x scale for low-res, 1.0x for high-res)
- SDL3-based pixel buffer management with separate rendering paths for text mode and graphics mode with text overlay
- Scaled character rendering using bitmap font data with proper bounds checking and background support
- Graphics infrastructure with setScreenMode(), renderGraphics(), renderTextOverlay(), and renderCharScaled() functions
 - WIDTH integration: changeWidth() adjusts text columns (40/80/132) and resizes SDL window when in text mode; ignored safely in console builds

## ⚠️ Partially Implemented

### String System (100% Complete)
- ✅ **String Descriptors**: Basic string representation with automatic memory management
- ✅ **String Literals**: Parsing and storage in expressions
- ✅ **Basic Operations**: String assignment and display
- ✅ **String Heap with Garbage Collection**: Automatic memory management with mark-compact GC, configurable policies, root provider pattern, and string protection
- ✅ **StringManager Interface**: High-level string operations including creation, concatenation, slicing (LEFT$, RIGHT$, MID$), search (INSTR), and comparison
- ✅ **Temporary String Management**: RAII-based temporary string pool with automatic cleanup
- ✅ **String Functions**: Complete integration of LEN, MID$, LEFT$, RIGHT$, INSTR, CHR$, STR$, VAL, ASC implemented with proper StringManager integration
- ✅ **String Arrays**: Multi-dimensional string storage with full ArrayManager integration
- ✅ **Runtime Integration**: Complete StringFunctionProcessor providing unified interface between ExpressionEvaluator and runtime StringManager components
- ✅ **Expression-Runtime Bridge**: Seamless conversion between expr::Value and gwbasic::Value types with proper memory management

### File I/O System (95% Complete)
- ✅ **LOAD/SAVE**: Basic program file operations
- ✅ **Sequential Files**: Complete OPEN, CLOSE, INPUT#, PRINT# implementation with file mode support (INPUT/OUTPUT/APPEND), file number management (1-255), and proper error handling
- ✅ **Random Access OPEN**: OPEN statement with LEN parameter support for custom record lengths (e.g., LEN = 64, LEN = 256)
- ✅ **File Manager**: FileManager component providing file operations with mode validation and integrated error mapping
- ✅ **Tokenizer Integration**: Complete support for '#' operator and 'AS' keyword with proper spacing in LIST output
- ✅ **LEN Parameter Parsing**: Fixed tokenization issues with LEN=value syntax, supporting multiple = token formats for robust parsing
- ✅ **Random Access Operations**: FIELD mapping, LSET/RSET, and GET/PUT for random-access records with proper buffer management and GW-BASIC-compatible padding/trim semantics
- ✅ **Directory Operations**: FILES (wildcards and optional path), KILL (file delete), NAME (rename with AS) implemented with filesystem error mapping to GW-BASIC error codes

### Input/Output (85% Complete)
- ✅ **PRINT**: Basic output with some formatting
- ✅ **PRINT USING**: Formatted numeric output with comprehensive pattern support (###.##, comma separators, currency symbols, sign indicators, asterisk fill)
- ✅ **INPUT**: Complete user input implementation with prompt support, type coercion, cross-platform handling (console stdin/GUI SDL events), and test mode support
- ✅ **File I/O**: Sequential file operations (OPEN, CLOSE, INPUT#, PRINT#) with complete file mode support and proper tokenizer integration
- ❌ **Device I/O**: Printer (LPRINT), communications ports
 
Recent fixes:
- PRINT and PRINT USING stop at 0x00 terminator and respect GW-BASIC newline rules
- Trailing semicolon suppresses newline; commas align to tab stops
- INPUT prompts are emitted via callback only to avoid duplicate output

### Graphics and Sound (70% Complete)
- ✅ **SCREEN Statement**: Complete implementation with 14 video modes (0-13), dynamic window resizing, and proper graphics mode switching
- ✅ **Text Framebuffer**: Scaled text overlay system for graphics modes with automatic character scaling based on resolution
- ✅ **Graphics Infrastructure**: SDL3-based pixel buffer management with separate rendering paths for text and graphics modes
- ✅ **COLOR Statement**: Complete implementation with foreground and background color support (0-15 foreground, 0-7 background) using standard CGA/EGA/VGA 16-color palette
  - Border argument is parsed for compatibility and ignored; tokenizer-aware commas supported; immediate-mode handler consumes trailing tokens to prevent stray syntax errors
- ✅ **Graphics Drawing**: Complete PSET, LINE, CIRCLE implementation with coordinate parsing, color support, and proper tokenization handling
- ✅ **GraphicsContext**: Full graphics drawing API with Bresenham line algorithm, midpoint circle algorithm, and SDL3 pixel buffer integration
- ⚠️ **Advanced Graphics**: GET, PUT statements for sprite/image manipulation
  - File GET/PUT for random-access records is complete (see File I/O System)
  - Graphics GET/PUT is partially implemented: supports capturing/putting rectangular blocks, but uses a simplified array format and default mode; action verbs and full GW-BASIC block format are not yet compatible
- ❌ **Sound**: SOUND, PLAY, BEEP statements

## ❌ Not Implemented

### Language Gaps
- ❌ **String Manipulation**: Full string function library
- ❌ **Advanced Math**: Complex math functions, matrix operations
- ❌ **Time/Date**: TIME$, DATE$ functions
- ❌ **System Integration**: SHELL, ENVIRON$

### Editor Features
- ❌ **Full Screen Editor**: Advanced editing capabilities
- ✅ **Function Keys**: Complete F1-F10 soft key support with default assignments and event trap integration
- ❌ **Auto Features**: AUTO line numbering, RENUM (utilities exist in ProgramStore; commands not wired)
- ❌ **List Formatting**: Advanced LIST options

### Compatibility Features
- ❌ **Protected Files**: Encrypted BASIC program format
- ❌ **Binary Data**: BSAVE, BLOAD operations
- ❌ **Cassette I/O**: CSAVE, CLOAD (historical)

## 🧪 Test Coverage

### Unit Tests (Good Coverage)
- ✅ **Tokenizer**: 304 lines of tests, comprehensive token recognition
- ✅ **ProgramStore**: 214 lines of tests, all major operations covered
- ✅ **InterpreterLoop**: 217 lines of tests, execution flow tested
- ✅ **BasicDispatcher**: 130 lines of tests, statement execution verified
- ✅ **NumericEngine**: 99 lines of tests, arithmetic operations validated
- ✅ **MBF Compatibility**: 88 test assertions, IEEE↔MBF conversion verified
- ✅ **ExpressionEvaluator**: 120 lines of tests, expression parsing and array access checked
- ✅ **Runtime Components**: Variable table and runtime stack tested
- ✅ **String Heap**: 105 test assertions, automatic GC and memory management validated
- ✅ **String Manager**: 120 test assertions, string operations and temporary pool verified
- ✅ **Event Traps**: 8 test assertions, event trap system and timer handling validated

### Integration Tests (Basic Coverage)
- ✅ **GOSUB/RETURN**: Subroutine call mechanism
- ✅ **LOAD Operations**: Program loading from files
- ✅ **FOR/NEXT Loops**: Loop execution with various parameters

### Test Status: **All tests passing** (7 runtime tests + 1 array manager test + 1 event traps test + 1 error handling test + 1 string functions test = 11 total runtime tests, 425+ total assertions)

**Recent Additions:**
- ✅ **String Heap Tests**: 105 test assertions covering automatic GC, root provider integration, memory statistics, allocation failure handling
- ✅ **String Manager Tests**: 120 test assertions covering string operations, temporary pool management, RAII helpers, error conditions
- ✅ **Array Manager Tests**: 25 test assertions covering array creation, element access/modification, multi-dimensional arrays, bounds checking, and StringRootProvider integration
- ✅ **Event Traps Tests**: 8 test assertions covering event trap configuration, timer events, key events, error events, and trap enabling/disabling
- ✅ **Error Handling Tests**: 25 test assertions covering ON ERROR GOTO, RESUME variants, ERL/ERR variables, ERROR statement simulation, RESUME without error validation, and comprehensive error handler management
- ✅ **String Functions Tests**: 75+ test assertions covering integrated string function system with StringFunctionProcessor, proper memory management, expression-runtime conversion, and comprehensive function call interface
- ✅ BasicDispatcher tests for SYSTEM (extended 0xFE) halting semantics and PRINT separator/newline behavior
- ✅ Regression tests ensuring 0xFFFF halt sentinel propagation prevents spurious syntax errors
 - ✅ WIDTH tests: validate WIDTH 40/80 success cases and invalid width error handling

## 📊 Completion Estimates

| Component | Completion | Lines of Code | Status |
|-----------|------------|---------------|---------|
| Tokenizer | 100% | ~800 | Stable |
| Program Store | 95% | ~600 | Stable |
| Expression Evaluator | 98% | ~1000 | Almost Complete |
| Numeric Engine | 100% | ~1200 | Complete |
| Runtime System | 100% | ~1750 | Complete |
| Interpreter Loop | 90% | ~350 | Core Complete |
| Basic Dispatcher | 100% | ~1700 | Complete |
| User Interface | 90% | ~700 | Well Featured |
| **Overall** | **96%** | **~8100** | **Release Candidate** |

## 🎯 Next Priority Items

### High Priority (Core Language Completion)
1. **Advanced Graphics (Graphics GET/PUT)**: Implement full GW-BASIC-compatible block format, array layout, STEP handling, and action verbs (PSET, PRESET, AND, OR, XOR) for graphics PUT/GET

### Medium Priority (Language Features)
1. **Time/Date Functions**: TIME$ and DATE$ implementation
2. **PRINT USING String Patterns**: Extend PRINT USING to support string formatting patterns
3. **Sound**: SOUND and BEEP statements

### Low Priority (Advanced Features)
1. **Protected Files**: Encrypted program format
2. **Device I/O**: Printer (LPRINT), communications ports
3. **Binary Data**: BSAVE, BLOAD operations

## 🚧 Known Issues

### Critical Issues
- None identified at this time

### Important Issues  
- Limited numeric formatting options beyond PRINT USING
- Graphics GET/PUT uses simplified array format and default mode; not yet compatible with GW-BASIC block format

### Minor Issues
- Trace output needs improvement
- Documentation could be more comprehensive
- Some test coverage gaps

## 🎮 Current Capabilities

The reimplemented GW-BASIC can currently run simple programs such as:

```basic
10 DIM A(10), MATRIX(5,5)
15 SCREEN 1: REM Graphics mode with text overlay
20 COLOR 14: REM Yellow text
25 PRINT "Hello, World!"
30 COLOR 2, 1: REM Green text on blue background
35 INPUT "Enter your name: ", NAME$
40 DEF FN AREA(L, W) = L * W
45 DEF FN SQUARE(X) = X * X
50 OPEN "data.txt" FOR OUTPUT AS #1
55 PRINT #1, "User: " + NAME$
60 FOR I = 1 TO 10
65   A(I) = I * 2
70   PRINT #1, "Number: "; I; " Array: "; A(I)
75   PRINT "Number: "; I; " Array: "; A(I)
76   PSET (I * 5, 50), I MOD 16: REM Draw colored pixels
80 NEXT I
85 CLOSE #1
86 REM Open random access file with custom record length
87 OPEN "records.dat" FOR RANDOM AS #2 LEN = 64
88 CLOSE #2: REM Close random access file
90 MATRIX(2,3) = 42
95 COLOR 15, 0: REM White text on black background
100 IF MATRIX[2,3] > 40 THEN PRINT "Matrix element is large!"
105 PRINT "Area of 5x3 rectangle: "; FN AREA(5, 3)
110 PRINT "Square of 7: "; FN SQUARE(7)
115 PRINT USING "Currency: $###.##"; 123.45
120 PRINT USING "Percentage: +##.#%"; 95.7
125 PRINT USING "With commas: ###,###.##"; 12345.67
130 OPEN "data.txt" FOR INPUT AS #2
135 INPUT #2, FILEDATA$
140 PRINT "Read from file: "; FILEDATA$
145 CLOSE #2
146 LINE (10, 10)-(100, 100), 14: REM Draw a line
147 CIRCLE (160, 100), 50, 12: REM Draw a circle
150 SCREEN 0: REM Back to text mode
155 END
```

It supports:
- Variable assignment and arithmetic
- FOR-NEXT loops with STEP
- IF-THEN-ELSE conditionals  
- GOTO and GOSUB/RETURN
- Basic PRINT statements with correct semicolon/comma newline rules
- INPUT statements with prompt support (callback-driven) and type coercion
- PRINT USING formatted output with numeric patterns, currency symbols, comma separators, and sign indicators
- Array element access with both A(I) and A[I] syntax, including multi-dimensional arrays
- **User-defined functions**: DEF FN statement support with function definition and FN function call syntax
- Program LOAD/SAVE operations
- **Sequential file I/O**: OPEN, CLOSE, PRINT#, INPUT# with file mode support (INPUT/OUTPUT/APPEND)
- **Random access file support**: OPEN with LEN for custom record lengths, plus FIELD/LSET/RSET mapping and GET/PUT for record I/O
- Interactive command shell; SYSTEM halts program as an extended statement
- Function key support (F1-F10) with soft key expansion and event trap integration
- Microsoft Binary Format (MBF) compatibility for numeric accuracy
- Complete SCREEN statement with 14 video modes (0-13) and dynamic window resizing
- Text display in all graphics modes with automatic character scaling and proper text overlay
- Complete COLOR statement with 16-color palette support for foreground and background colors
- **Graphics drawing**: PSET, LINE, CIRCLE statements with coordinate parsing, color support, and proper tokenization handling
- **Complete error handling**: ON ERROR GOTO, RESUME, ERROR statement implementation with proper error routing, ERL/ERR variables, and comprehensive error handler management
- **Enhanced error reporting**: Syntax errors now include proper line number information for better debugging
 - WIDTH statement to switch text columns among 40, 80, or 132 (GUI resizes window accordingly in text mode)

## 🔮 Future Roadmap

### Phase 1: Language Completion (Target: Q4 2025)
- ✅ Complete string function integration with runtime system
- Flesh out graphics GET/PUT to match GW-BASIC block format

### Phase 2: I/O and Formatting (Target: Q1 2026)
- Implement time/date functions (TIME$, DATE$)
- Improve PRINT USING string pattern coverage
- Expand device I/O (e.g., LPRINT) if feasible

### Phase 3: Advanced Features (Target: Q2 2026)
 - Enhance graphics system (sprite/block format compatibility for GET/PUT, palette/attribute nuances, performance)
 - Implement sound capabilities
 - Expand event trap system and diagnostics
 - Improve GW-BASIC compatibility edge cases

### Phase 4: Polish and Optimization (Target: Q3 2026)
- Performance optimizations
- Enhanced debugging features
- Complete documentation
- Extensive testing

## 🤝 Contributing

The project welcomes contributions in several areas:

**High Impact Areas:**
- Graphics GET/PUT (full GW-BASIC block format and action verbs)
- Device I/O (Printer/COM) groundwork

**Medium Impact Areas:**
- Time/Date functions (TIME$, DATE$)
- PRINT USING string pattern coverage
- Sound capabilities (SOUND, BEEP)

**Getting Started:**
1. Check the `docs/reimplementation_guide.md` for architecture overview
2. Review existing tests for examples
3. Each module has its own README with implementation details
4. Use `make check` to run the test suite

## 📝 Notes

- The implementation prioritizes behavior compatibility over internal architecture matching
- Modern C++ features are used while maintaining GW-BASIC semantics
- SDL3 provides cross-platform graphics and input handling
- Autotools ensures portable building across platforms
- Comprehensive test suite validates functionality

**Status Legend:**
- ✅ Complete and tested
- ⚠️ Partially implemented
- ❌ Not implemented
- 🧪 Tested feature
- 🚧 Known issues exist
