# GW-BASIC Reimplementation Guide

A practical blueprint to rebuild the 8086 GW-BASIC in another language, preserving core behavior and quirks.

## Goals

- Replicate language semantics, I/O, and formatting visible to users.
- Modularize into tokenizer, interpreter, evaluator, runtime, devices, and editor.
- Use host OS APIs but preserve observable behavior (CRLF, PRINT USING, INPUT parsing, random-file semantics, event traps).

## Architecture overview

- Tokenizer (cruncher)
  - Input: raw source line. Output: token bytes + `0x00` terminator.
  - Tables: reserved words/operators (from `IBMRES.ASM`), extended FE-prefixed tokens.
  - Embeds small integer constants as compact forms; preserves quoted strings.

- Program store
  - Linked list of tokenized lines, sorted by line number. Line layout: `link(2) lineNo(2) tokens... 0`.
  - Operations: insert/replace/delete by line number; scan via link fields.

- Interpreter loop + statement dispatch
  - Fetch next token; dispatch via table to handlers (`PRINT`, `LET`, `IF`, `FOR/NEXT`, `GOTO/GOSUB/RETURN`, `INPUT`, `READ/RESTORE`, etc.).
  - `NEWSTT`-like entry point handles event traps and Ctrl-C.

- Expression evaluator
  - Precedence parser with numeric/string types. Coercions to match GW-BASIC.
  - Intrinsics: numeric (`LOG`, `INT`, `RND`, `ABS`, `SGN`, `SQR`, etc.) and string (`LEN`, `MID$`, `LEFT$`, `RIGHT$`, `INSTR`, `CHR$`, `STR$`, `VAL`).

- Numeric engine
  - FAC/DFAC model with SP/DP; normalization/rounding compatible with GW printing and `VAL`.
  - `PRINT USING` and general number formatting (leading zero suppression, exponent layout, commas/decimal point handling).

- Strings and arrays
  - String descriptors (len, ptr). Temp descriptor pool for expressions. GC when temp pool exhausted or on demand.
  - Arrays with descriptors and bounds; `DEF*` default-type table (`DEFTBL`) drives variable typing.

### C++ mapping (reference skeleton)

- Types
  - `StrDesc { uint16_t len; uint8_t* ptr; }` in `src/Runtime/StringTypes.hpp` for short strings (≤255 chars).
  - `TempStrPool` holds temporary string descriptors produced during expression evaluation.
  - `StringHeap` in `src/Runtime/StringHeap.hpp` implements a grow-down arena with deterministic compacting GC given a root set.
  - `Array`/`Dim` in `src/Runtime/ArrayTypes.hpp` model typed arrays; for string arrays, `data` stores contiguous `StrDesc` entries; characters reside in `StringHeap`.

- GC integration
  - Collect roots: variable table, string array elements, runtime stacks (FOR/GOSUB/ERR), evaluator temps (`TempStrPool`).
  - Trigger `heap.compact(roots)` on allocation failure, `FRE`, `CLEAR`, and optionally at statement boundaries.
  - Post-compact, `StrDesc.ptr` are updated in-place; copying is done in root order.

- LHS string assignment helpers
  - Use `overwrite_left/right/mid(...)` from `StringHeap.hpp` to implement `LEFT$()/RIGHT$()/MID$()` assignment without changing target length.

- Arrays
  - After setting `rank` and `dims[k].lb/ub`, call `finalizeStrides(Array&)` so rightmost index varies fastest.
  - Access: `elemPtr(subscripts)`; for `String` arrays, cast to `StrDesc*` and assign new heap-allocated strings.
  - `ERASE` should clear string descriptors (len=0, ptr=null) and optionally trigger GC.

- Devices and files
  - Device registry and dispatch interface: init/term, OPN/CLS, SIN/SOT, RND, EOF/LOC/LOF, GPS/GWD, SCW/GCW.
  - Disk files: sequential text/binary, random access records, `KILL/NAME/FILES`, append semantics.

- Screen/editor
  - OS-agnostic editor: wrapping, scrolling, cursor navigation, logical vs physical lines via a terminator table; optional function-key label line.

- Event traps/time/date
  - `ON KEY/COM/PEN/STRIG GOSUB` support via an event-flag table; TIME$/DATE$ bridged to host.

## Data formats and compatibility

- Floats: Microsoft Binary Format (MBF) vs host IEEE 754. Internally you may use IEEE; ensure text I/O (VAL/STR$/PRINT/PRINT USING) matches GW outputs via custom conversion/rounding.
- Strings: descriptors; direct-mode input buffer `BUF`; program tokenizer buffer `KBUF`; DATA pointer semantics (`DATPTR`) must match READ/RESTORE.
- Program lines: stored zero-terminated; CR/LF only on device output.

## Minimal contracts (pseudocode)

- CHRGTR: fetch next byte from current line; if zero, hop to next line via link.
- FRMEVL: evaluate expression, set `VALTYP`, leave result in FAC (or string descriptor).
- NEWSTT: top-level statement execution; checks event flags; handles `RESUME` and error recovery via saved stack/text pointers.

## Implementation plan (phased)

1) Core state and memory
- Program store, pointers (`TXTTAB`, `CURLIN`, `DATPTR`), `VALTYP`, FAC/DFAC, string space, temp descriptors, `DEFTBL`.

2) Tokenizer and lister
- Implement reserved-word tables and cruncher; implement LIST to round-trip tokens to text. Cover FE two-byte tokens and embedded small integers.

3) Loop and basic statements
- `PRINT`, `LET`, `END`, `STOP`, then `IF/THEN`, `GOTO`, `GOSUB/RETURN`, `FOR/NEXT` with stack frames.

4) Evaluator and numerics
- Operators and precedence; numeric conversions; `INT`, `ABS`, `SGN`, `SQR`, `LOG`, `RND`.
- `PRINT USING` formatting parity (widths, rounding, exponent form).

5) Strings and arrays
- String ops and GC; arrays and default typing.

6) Devices/files
- Screen and keyboard via stdio; disk sequential; later random I/O and directory ops (`FILES`, `KILL`, `NAME`).

7) Editor
- Terminal-based line editor matching `SCNDRV` semantics where practical.

8) Events/time/date
- Event traps and TIME$/DATE$ as needed.

## Testing strategy

- Golden tests against known outputs for numerics (`PRINT USING`, extreme values), INPUT parsing (quoted strings, commas/colons), control flow (nested loops, ON GOTO/GOSUB, RESUME), file I/O (Ctrl-Z EOF, random records), and screen width/CRLF.
- Fuzzing: generate token streams constrained by syntax to stress evaluator and tokenizer.

## Notes and assumptions

- Behavior-first: replicate visible outputs even if internal encodings differ.
- Token set is the source of truth for both cruncher and lister; keep in one table.
- Editor can be approximated initially; exact key sequences and function-key labels are optional unless you target identical UX.

## Quick checklist

- [ ] Tokenizer + lister parity
- [ ] Core statements + evaluator + numerics
- [ ] Program store + shell (edit/list)
- [ ] Sequential file I/O + screen device
- [ ] Strings/arrays with GC
- [ ] Optional: random I/O, events, protected save/load, full editor

## Adjacent references

- See `docs/architecture_map.md` for subsystem layout.
- See `docs/appendix_tokens.md` for tokenizer/lister tokens.
- See `docs/appendix_mbf_and_rounding.md` for MBF formats and exact PRINT/VAL rounding/formatting behavior.

## Numeric I/O parity checklist

- VAL: literals with many digits crossing int→SP→DP transitions, with/without decimal points, with exponent parts (E±n), and with `%`, `!`, `#` suffixes.
- PRINT free‑format: values exactly at x.5 ulp boundaries, small magnitudes (< 0.01), large magnitudes near the changeover to E format.
- PRINT USING fixed: fields that require internal rounding (drop right‑side digits), and fields that overflow (expect leading `%`). Verify commas, trailing/leading zeros, and sign/`$` options.
- Double‑precision guard: cases where adding .5 would overflow; ensure divide‑by‑10 and exponent adjust path is triggered.
