Always ask for permission before making changes you weren't explicitly asked to make.

Write C++20 compatible code.

Put classes in namespace "gw".

Header files should end in ".hpp".

Do not omit "this" when referencing member variables.

Use "#pragma once" instead of header guards.

Use catch2 for tests.

The original 6502 assembly language source code is in /asm.

## Lessons Learned from GOSUB/RETURN Implementation

### File Organization
- **NEVER** create temporary or debug files in the project root directory
- Use `/tmp` or other temporary directories for debugging files
- Always clean up temporary files when done
- Place new tests in appropriate subdirectories (`tests/`, `tests/integration/`)
- Update `Makefile.am` files when adding new test programs

### Architecture Patterns
- Study existing assembly code in `/asm` to understand original behavior patterns
- The `RuntimeStack` class already provides proper support for GOSUB/RETURN frames
- Follow existing statement dispatch patterns in `BasicDispatcher::handleStatement()`
- Use existing jump infrastructure (like GOTO) for consistent control flow handling

### Tokenization and Program Storage
- `ProgramStore` expects statement tokens only, not including line number prefixes
- When using `tokenizer->crunch()` with line numbers, strip the `0x0D <low> <high>` prefix before storing
- The `InterpreterLoop` passes raw statement tokens to the `StatementHandler`
- Test both individual statement functionality and integration scenarios

### Testing Strategy
- Start with unit tests for individual statement functionality
- Add integration tests that exercise the complete control flow
- Test error conditions (e.g., "RETURN without GOSUB")
- Use existing test patterns (like `crunchStmt` in `test_basicdispatcher.cpp`)
- Verify tests pass at each level before adding complexity

### Code Quality
- Handle unused parameters with comment syntax: `/*paramName*/` 
- Provide proper error messages with context when possible
- Follow existing error handling patterns using `expr::BasicError`
- Maintain consistency with existing function signatures and return patterns