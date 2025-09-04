# Tokens and Reserved Words Appendix (derived from IBMRES.ASM)

This appendix summarizes the token set and reserved words used by the interpreter. It is derived from the `asm/IBMRES.ASM` tables (ALPTAB/RESLST for base tokens, SPCTAB for single‑char operators, and the extended sets for FE/FD/FF prefixes).

## Token encoding at a glance

- One‑byte tokens
  - Base statements, operators, and some keywords are single bytes in the token stream.
- Two‑byte tokens with prefixes
  - 0xFE (octal 376): extended statements (FE + index into STMDSX)
  - 0xFF (octal 377): standard functions (FF + index into FUNDSP)
  - 0xFD (octal 375): extended/auxiliary functions (FD + index into FUNDSX)
- Special character token map: see SPCTAB; certain single characters map to operators/tokens.

Tokenizer and lister notes
- The tokenizer (“cruncher”) matches reserved words using alphabetic dispatch tables (`ALPTAB`/`RESLST`). Multi‑word forms like “GO TO” are handled explicitly.
- `SPC()` and `TAB()` are recognized and encoded as tokens (`SPCTK`, `TABTK`).
- Single quote `'` is tokenized as the REM form (`SNGQTK`).
- Extended tables (`ALPTAX`/`RESLSX`) drive FE/FD processing for extended keywords.

---

## Statements (single‑byte unless noted)

- END, FOR, NEXT, DATA, INPUT, DIM, READ, LET, GOTO, RUN, IF, RESTORE, GOSUB, RETURN, REM, STOP, PRINT, CLEAR, LIST, NEW, ON, WAIT, DEF, POKE, CONT, OUT, LPRINT, LLIST, WIDTH, ELSE, TRON, TROFF, SWAP, ERASE, EDIT, ERROR, RESUME, DELETE, AUTO, RENUM, DEFSTR, DEFINT, DEFSNG, DEFDBL, LINE, WHILE, WEND, CALL, WRITE, OPTION, RANDOMIZE, OPEN, CLOSE, LOAD, MERGE, SAVE, COLOR, CLS, MOTOR, BSAVE, BLOAD, SOUND, BEEP, PSET, PRESET, SCREEN, KEY, LOCATE
- Notes
  - KEY also has a defined two‑byte token form (`$KEY2B`) for certain KEY features.
  - NEW (a.k.a. SCRATCH in some tables) is present as a statement.

### Numeric codes (one‑byte statement tokens)

The base statement tokens are assigned sequential one‑byte codes starting at 0x80 in the order defined by `STMDSP`. Derived mapping for this source:

- 0x80 END
- 0x81 FOR
- 0x82 NEXT
- 0x83 DATA
- 0x84 INPUT
- 0x85 DIM
- 0x86 READ
- 0x87 LET
- 0x88 GOTO
- 0x89 RUN
- 0x8A IF
- 0x8B RESTORE
- 0x8C GOSUB
- 0x8D RETURN
- 0x8E REM
- 0x8F STOP
- 0x90 PRINT
- 0x91 CLEAR
- 0x92 LIST
- 0x93 NEW
- 0x94 ON
- 0x95 WAIT
- 0x96 DEF
- 0x97 POKE
- 0x98 CONT
- 0x99 (reserved padding → SNERR)
- 0x9A (reserved padding → SNERR)
- 0x9B OUT
- 0x9C LPRINT
- 0x9D LLIST
- 0x9E (reserved padding → SNERR)
- 0x9F WIDTH
- 0xA0 ELSE
- 0xA1 TRON
- 0xA2 TROFF
- 0xA3 SWAP
- 0xA4 ERASE
- 0xA5 EDIT
- 0xA6 ERROR
- 0xA7 RESUME
- 0xA8 DELETE
- 0xA9 AUTO
- 0xAA RENUM
- 0xAB DEFSTR
- 0xAC DEFINT
- 0xAD DEFSNG
- 0xAE DEFDBL
- 0xAF LINE
- 0xB0 WHILE
- 0xB1 WEND
- 0xB2 CALL
- 0xB3 (reserved padding → SNERR)
- 0xB4 (reserved padding → SNERR)
- 0xB5 (reserved padding → SNERR)
- 0xB6 WRITE
- 0xB7 OPTION
- 0xB8 RANDOMIZE
- 0xB9 OPEN
- 0xBA CLOSE
- 0xBB LOAD
- 0xBC MERGE
- 0xBD SAVE
- 0xBE COLOR
- 0xBF CLS
- 0xC0 MOTOR
- 0xC1 BSAVE
- 0xC2 BLOAD
- 0xC3 SOUND
- 0xC4 BEEP
- 0xC5 PSET
- 0xC6 PRESET
- 0xC7 SCREEN
- 0xC8 KEY
- 0xC9 LOCATE

Notes
- Codes above reflect the ordering including reserved DUMMY slots that map to `SNERR`.
- Non‑statement keywords (e.g., THEN/TO) continue numbering after this range in `IBMRES.ASM`; exact one‑byte values depend on the table counts and are build‑specific. See below for two‑byte encodings which are stable.

## Non‑statement keywords (single‑byte)

These appear in expressions or as syntactic markers:

- THEN, TO, TAB (function‑like), STEP, USR, FN, SPC (function‑like), NOT, ERL, ERR, STRING$, USING, INSTR, ' (single‑quote as REM), VARPTR, CSRLIN, POINT, OFF, INKEY$

## Operators (from SPCTAB and token list)

Single‑char operators map directly via `SPCTAB`:

- + (PLUSTK), - (MINUTK), * (MULTK), / (DIVTK), ^ (EXPTK), \ (IDIVTK, integer division), ' (SNGQTK → REM), > (GREATK), = (EQULTK), < (LESSTK)

Multi‑char logical/relational operators are tokenized as keywords:

- AND, OR, XOR, EQV, IMP, MOD

## Standard functions (two‑byte tokens with 0xFF prefix)

- LEFT$, RIGHT$, MID$, SGN, INT, ABS, SQR, RND, SIN, LOG, EXP, COS, TAN, ATN, FRE, INP, POS, LEN, STR$, VAL, ASC, CHR$, PEEK, SPACE$, OCT$, HEX$, LPOS, CINT, CSNG, CDBL, FIX, PEN, STICK, STRIG, EOF, LOC, LOF
- Notes
  - `ONEFUN` marks the first function token in the table. `MIDTK`, `SQRTK`, `ATNTK` denote specific functions used elsewhere.
  - The lister uses the FF prefix when reconstructing function names.

### Numeric codes (FF prefix)

Encoding is two bytes: `FF nn`, where `nn` is a 0‑based index into `FUNDSP` as listed here.

- FF 00 LEFT$
- FF 01 RIGHT$
- FF 02 MID$
- FF 03 SGN
- FF 04 INT
- FF 05 ABS
- FF 06 SQR
- FF 07 RND
- FF 08 SIN
- FF 09 LOG
- FF 0A EXP
- FF 0B COS
- FF 0C TAN
- FF 0D ATN
- FF 0E FRE
- FF 0F INP
- FF 10 POS
- FF 11 LEN
- FF 12 STR$
- FF 13 VAL
- FF 14 ASC
- FF 15 CHR$
- FF 16 PEEK
- FF 17 SPACE$
- FF 18 OCT$
- FF 19 HEX$
- FF 1A LPOS
- FF 1B CINT
- FF 1C CSNG
- FF 1D CDBL
- FF 1E FIX
- FF 1F PEN
- FF 20 STICK
- FF 21 STRIG
- FF 22 EOF
- FF 23 LOC
- FF 24 LOF

## Extended statements (two‑byte tokens with 0xFE prefix)

- FILES, FIELD, SYSTEM, NAME, LSET, RSET, KILL, PUT, GET, RESET, COMMON, CHAIN, DATE$, TIME$, PAINT, COM, CIRCLE, DRAW, PLAY, TIMER, ERDEV, IOCTL, CHDIR, MKDIR, RMDIR, SHELL, ENVIRON, VIEW, WINDOW, PMAP, PALETTE, LCOPY, CALLS
- Notes
  - `$COM2B` defines the two‑byte value used for `COM` extended token.
  - DATE$ and TIME$ appear as extended statements but also have function forms handled in evaluation hooks.

### Numeric codes (FE prefix)

Encoding is two bytes: `FE nn`, where `nn` is a 0‑based index into `STMDSX` as listed here.

- FE 00 FILES
- FE 01 FIELD
- FE 02 SYSTEM
- FE 03 NAME
- FE 04 LSET
- FE 05 RSET
- FE 06 KILL
- FE 07 PUT
- FE 08 GET
- FE 09 RESET
- FE 0A COMMON
- FE 0B CHAIN
- FE 0C DATE$
- FE 0D TIME$
- FE 0E PAINT
- FE 0F COM
- FE 10 CIRCLE
- FE 11 DRAW
- FE 12 PLAY
- FE 13 TIMER
- FE 14 ERDEV
- FE 15 IOCTL
- FE 16 CHDIR
- FE 17 MKDIR
- FE 18 RMDIR
- FE 19 SHELL
- FE 1A ENVIRON
- FE 1B VIEW
- FE 1C WINDOW
- FE 1D PMAP
- FE 1E PALETTE
- FE 1F LCOPY
- FE 20 CALLS

## Extended/auxiliary functions (two‑byte tokens with 0xFD prefix)

- CVI, CVS, CVD, MKI$, MKS$, MKD$, KTN, JIS, KPOS, KLEN
- Notes
  - These include binary/string conversion functions and locale/kanji helpers present in some builds.

### Numeric codes (FD prefix)

Encoding is two bytes: `FD nn`, where `nn` is a 0‑based index into `FUNDSX` as listed here.

- FD 00 CVI
- FD 01 CVS
- FD 02 CVD
- FD 03 MKI$
- FD 04 MKS$
- FD 05 MKD$
- FD 06 KTN
- FD 07 JIS
- FD 08 KPOS
- FD 09 KLEN

## Alphabetic dispatch and reserved word lists

- `ALPTAB` → per‑letter tables (A..Z) into `RESLST`, each a sequence of encoded names ending with 0; high‑bit marks end of a single reserved word entry, followed by a token id byte.
- Special cases encoded in tables:
  - “GO TO” recognized as an alternate for GOTO.
  - `SPC(` and `TAB(` forms map to their tokens via an inline reference to `SPCTK`/`TABTK`.

## Special character token map (SPCTAB)

The SPCTAB table maps byte values to token ids. Highlights:

- '+' → PLUSTK
- '-' → MINUTK
- '*' → MULTK
- '/' → DIVTK
- '^' → EXPTK
- '\\' → IDIVTK (integer division)
- '\'' → SNGQTK (single‑quote → REM)
- '>' → GREATK
- '=' → EQULTK
- '<' → LESSTK

Note: These are trigger characters; their one‑byte token codes are assigned in the unified token space following `IBMRES.ASM`’s ordering and may vary across builds. Two‑byte encodings (FE/FF/FD) above are stable by index.

## Implementation tips (tokenizer/lister)

- Tokenizer
  - Use `ALPTAB` dispatch based on uppercased leading letter; match the longest reserved word sequence before accepting.
  - Handle GO TO as a special match; recognize `SPC(` and `TAB(` to emit their tokens.
  - Emit prefixes: 0xFE for extended statements, 0xFF for standard functions, 0xFD for extended functions.
- Lister
  - Reverse the mapping using the same tables (`RESLST` for base, `RESLSX` for extended). When a prefix byte (FE/FF/FD) is seen, read the following index to locate the correct lexeme.
  - For single‑quote REM, print `'` and the remainder of the line.

---

This appendix is intended for implementers of the tokenizer and lister to ensure token parity with the original tables in `IBMRES.ASM`. It focuses on categories and mapping behavior rather than numeric token values to keep it portable across builds.
