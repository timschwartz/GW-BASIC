# GW-BASIC 8086 Architecture Map

This document maps the major subsystems found under `asm/` and explains how they fit together, so you can rebuild the system in another language while preserving behavior.

## Top-level runtime

- `GWMAIN.ASM`
  - Main interpreter loop, direct-mode and program execution, error handling, and statement dispatch. Implements scanning/token crunching of input lines, storage format, and runtime control flow (`NEWSTT`, `GONE`, `SCNEXT`).
  - Implements statement handlers for core constructs (IF/THEN, FOR/NEXT, ON GOTO/GOSUB, INPUT, READ/DATA, RESTORE, AUTO, RENUM, OPTION BASE, etc.), and utilities like scanning strings and constants.
  - Collaborates with the expression evaluator (`GWEVAL.ASM`) and printing/formatting (numeric/string).

- `GWEVAL.ASM`
  - Expression parser/evaluator (numeric and string). Responsible for token-sensitive parsing, operator precedence, type checks/conversions, and calling math/string primitives.
  - Uses the floating accumulator (FAC) and `VALTYP` to carry values across operations.

- `GWINIT.ASM`
  - Initialization: memory layout setup, stack/heap pointers, device setup, and initial defaults (`OPTION BASE`, `DEFTBL`).

- `GWLIST.ASM`
  - LIST/LLIST and listing-related scanning/token recognition (including extended tokens). A mirror of the tokenizer to re-hydrate tokenized lines for display.

- `GWSTS.ASM`
  - Shared statement support: CLS, LOCATE, WIDTH, COLOR, SCREEN, GETLIN, event-trap infrastructure (ON KEY/COM/PEN/STRIG GOSUB), TIME$/DATE$ get/set, and parameter scanning helpers.

## Data, memory, and tokens

- `GWDATA.ASM`
  - Global variables, pointers, and buffers: program text (`TXTTAB`), memory boundaries (`FRETOP`, `STREND`, `ARYTAB`), temp string descriptors, krunch buffer (`KBUF`/`BUF`), device tables, error/control flags, `VALTYP`, FAC/DFAC, default-type table (`DEFTBL`).
  - Program-line and input buffers, function-key storage, and parameter-block storage for user-defined functions.

- `IBMRES.ASM`
  - Reserved word tables, operators, and alphabetic dispatch (`ALPTAB`). Extended two-byte tokens (FE-prefixed) and statement/function mappings.

## Device abstraction and I/O

- `GIOTBL.ASM`
  - Device registry. Maps device names to dispatch tables and to their init/term routines. The first dispatch entry is disk. Others include keyboard, screen, printer(s), COM ports, cassette (optional), and raw console.

- Keyboard: `GIOKYB.ASM`
  - Keyboard device driver: init/term, key queue, polling, INKEY$, soft-key expansion, two/three-byte scan sequence handling, and Ctrl-C/Ctrl-S logic.

- Screen device: `GIOSCN.ASM`
  - Screen output device wrapper for file I/O: width handling, CR/LF normalization, POS(), and per-file vs device width resolution.

- Screen editor/driver (OS-agnostic): `SCNDRV.ASM`
  - Logical vs physical line model, terminator table, wrapping/scanning, scrolling (up/down), cursor movement, control-char dispatch, and optional function-key label line.

- Disk: `GIODSK.ASM`
  - Disk device driver: sequential and random file I/O over DOS FCBs, buffer management, `FILES`/`KILL`/`NAME`/`RESET`/`SYSTEM`, fast load, and protected save/load encoding.

## Math, numeric I/O, and formatting

- `MATH2.ASM`
  - Floating-point conversions and printing, single/double multiply, INT and rounding, LOG, polynomial evaluation, PRINT USING, and numeric I/O (accumulating digits/exponents). Normalization and rounding for SP/DP.

- `MATH1.ASM`
  - Additional math primitives complementing `MATH2`.

## OEM and hardware adaptations

- `OEM.H` + `GIO86U` + `MSDOSU`
  - Configuration switches and OS/BIOS interfacing macros used across subsystems.

- Other `GIO*` modules (e.g., `GIOCOM.ASM`, `GIOCON.ASM`, `GIOLPT.ASM`, `GIOSCN.ASM`)
  - Device-specific dispatch tables and routines following a common interface (EOF/LOC/LOF/CLS/OPN/SIN/SOT/GPS/GWD/... ).

## Execution model and program storage

- Program text is tokenized (“crunched”) on entry. Stored as a linked list of lines:
  - 2-byte link to next line, 2-byte line number, tokenized bytes terminated by 0.
  - Tokenizer replaces reserved words with token bytes (1- or 2-byte tokens), embeds small integer constants, and preserves literals.
- Interpreter executes by advancing a text pointer through tokenized bytes, dispatching on tokens.

## Cross-module data structures (selected)

- FAC/DFAC: floating accumulator (single/double) with `VALTYP` denoting type and conversion helpers.
- FDB/FCB: file/device control blocks used by disk I/O for sequential/random access and buffering.
- Keyboard queue: ring buffer; soft-key tables; trap tables for ON GOSUB events.
- Screen editor state: window bounds, cursor (X/Y), line width, logical line terminators, insert mode, wrap and scroll state.

## Control flow highlights

- `NEWSTT` is the central entry for executing the next statement, managing ON GOSUB traps and Ctrl-C.
- Error handling funnels through message formatting, optional line-number printing, and program/direct-mode branching.
- `SCNDRV` handles on-screen editing and interactive input, while `GIOSCN` is the print path for device/file output respecting widths and CR/LF.

Use this map to locate reference behavior and entry points as you port functionality subsystem by subsystem.
