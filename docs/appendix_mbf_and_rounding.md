Microsoft Binary Format (MBF) and Rounding/Conversion Paths (PRINT and VAL)

This appendix documents the numeric representation used by GW‑BASIC and the exact normalization/rounding and conversion flows exercised by VAL (input) and PRINT/PRINT USING (output). The goal is bit‑for‑bit parity with the original interpreter.

Sources cross‑referenced: asm/MATH2.ASM (conversion, rounding, printing), asm/GWDATA.ASM (FAC/DFAC layout and working buffers).

MBF at a glance

- Format family: Microsoft Binary Format (MBF), base‑2 normalized, no NaNs or Infs, no subnormals.
- Exponent bias: 128 (octal 200). Stored exponent byte of 0 means the value is zero (mantissa ignored).
- Sign: stored in the high bit (0x80) of the highest mantissa byte. The exponent byte has no sign bit.
- Normalization: mantissa is normalized so that the implicit leading 1 lies just to the left of the binary point. That leading 1 is not stored; routines “install”/restore it during arithmetic.

Logical storage layouts (independent of the FAC scratch layout):

- Single precision (4 bytes = MBF32)
  - Byte 0: exponent (bias 128); 0 => zero
  - Byte 1: sign bit in 0x80, top 7 mantissa bits in 0x7F
  - Byte 2: next 8 mantissa bits
  - Byte 3: lowest 8 mantissa bits
  - Significant digits: ~7 decimal digits

- Double precision (8 bytes = MBF64)
  - Byte 0: exponent (bias 128); 0 => zero
  - Byte 1: sign bit in 0x80, top 7 mantissa bits in 0x7F
  - Bytes 2..7: remaining 48 mantissa bits (total 55 stored mantissa bits; 56th is implicit)
  - Significant digits: ~16 decimal digits

- Integer (16‑bit): signed two’s complement. Used during input accumulation and for integer variables/PRINT USING integer paths.

Notes
- Denormals are not represented. Underflow normalizes to zero ($ZERO for single, $DZERO for double).
- Overflow raises “Overflow” (ERROV) after exponent stepups during rounding/normalization.

Internal accumulators and scratch layout

The interpreter uses a working accumulator “FAC” for single and “DFAC” for double. You do not need to reproduce the exact byte layout to match MBF, but understanding it explains rounding and normalization:

- $FAC (single path scratch)
  - $FAC (byte): exponent (bias 128)
  - $FAC+1 (byte): sign as a byte (0x80 for negative, 0x00 for positive) used during ops
  - $FAC-1 (byte): high mantissa byte (sign bit is written here during packing)
  - $FACLO (word): low 16 bits of mantissa
  - AH is used as an overflow/guard byte during rounding

- $DFACL … $FAC-1 (double path scratch)
  - Mantissa spread across 7 bytes plus a dedicated “overflow byte” at $DFACL-1
  - $FAC exponent byte and $FAC+1 temporary sign mirror single path semantics

References
- Packing single: $ROUNS -> label PAKSP writes sign into $FAC-1
- Packing double: $ROUND places sign into $FAC-1 after carry propagation

Normalization

- Single precision: $NORMS
  - Left‑shifts (BL:DX:AH mantissa) until the high bit of BL is 1, decrementing the exponent.
  - If a whole‑byte left shift is possible without underflow, it shifts by 8 at a time for speed.
  - Underflow path jumps to $ZERO (exponent set to 0, number becomes 0).
  - On completion, jumps into $ROUNS to round and pack.

- Double precision: $NORMD
  - Left‑shifts the 56‑bit mantissa (with an 8‑bit overflow byte) until normalized, decrementing exponent.
  - Optimizes 8‑bit moves where possible.
  - Underflow path jumps to $DZERO.
  - On completion, jumps into $ROUND to round and pack.

Rounding: behavior and tie‑breaking

GW‑BASIC implements round‑to‑nearest, ties to even at the destination precision.

- Single precision rounding: $ROUNS
  - Keeps “guard” bits in AH. Before packing, it:
    - AND AH, 0o340 (keep guard region), then ADD AH, 0o200 to add 0.5 ulp at the rounding boundary.
    - If no carry from AH, no increment to the mantissa is needed.
    - If carry propagates, it increments DX (low 16 bits) and possibly BL (high byte). If BL overflows it shifts right and increments the exponent (overflow guarded by $OVFLS).
    - Tie to even: when exactly halfway, it clears the LSB of DL (AND DL, 0o376) so the final mantissa ends even.
  - Packs: writes BL|sign into $FAC-1, DX into $FAC-3…-2, exponent in $FAC.

- Double precision rounding: $ROUND
  - Adds 0.5 ulp into the overflow/guard region: ADD WORD PTR [$DFACL-1], 0o200 and propagates carries upwards through the 56‑bit mantissa.
  - If carry reaches the top, the exponent is incremented (overflow guarded by $OVFLS).
  - Tie to even: if the overflow byte ends up zero after propagation, it clears the least significant mantissa bit (AND BYTE PTR $DFACL, 0o376).
  - Finally sets/clears the sign bit in $FAC-1 from $FAC+1 (0x80 if negative).

Implications
- Both single and double use unbiased round‑to‑nearest‑even. To match printed digits or VAL’d results, ensure your port uses the same rounding at the same stage (see next sections for where rounding is applied in VAL and PRINT).

VAL (numeric input) conversion flow

Text‑to‑binary parsing is in $FIDIG + $FINEX + force/type fixups. High‑level behavior:

- Digit accumulation ($FIDIG)
  - Accumulates initial digits in 16‑bit integer ($FACLO) while it fits (threshold ~32767). This avoids early FP.
  - On overflow risk, converts to single ($CSI), continues as FP.
  - If continuing in single would lose precision (FAC ≥ 1,000,000), converts to double ($CDS) and continues there.
  - Decimal places counter: DI counts places to the right of the decimal point (incremented once decimal point seen). This is later used to apply the decimal exponent fix‑up.

- Exponent part ($FINEX)
  - Parses optional sign and decimal exponent into DX; negative exponent indicated via SI.

- Type forcing ($FINFC): suffixes and context
  - ! forces single ($FINS), # forces double ($FIND), % forces integer ($FINI/$FI).
  - Without a suffix, type is selected by context or defaults (see DEFTBL).

- Exponent fix‑up ($FINE, invoked via $FINS/$FIND)
  - Applies DI (fractional digit count) and the explicit E±exp to scale by powers of 10 using $MUL10, $DIV10, and the multi‑digit power routine MDPTEN.

- Final rounding/packing
  - Landing type determines rounding:
    - Integer: $CINC/$CIND path to integer with rounding that guards −32768 corner case.
    - Single: $NORMS → $ROUNS (nearest, ties to even) and pack to MBF32.
    - Double: $NORMD → $ROUND (nearest, ties to even) and pack to MBF64.

Edge cases to match
- Very long literals: the transition integer → single → double triggers as described; once in double, precision is preserved during digit accumulation until final rounding to target type.
- Negative zero does not exist; parsing “-0” yields +0 (exponent 0). Sign is only meaningful for non‑zero values.

PRINT/PRINT USING conversion flow (free format and fixed/E formats)

Top‑level control is in $FOUT (free format) and $PUFOT/$PUF (PRINT USING). Both converge on the same core routines.

Key stages

1) Sign handling (S)
- Emits a leading space by default; replaces with “-” for negatives. For PRINT USING, “+” can be forced, trailing sign supported, and floating “$”.

2) Choose free/fixed/E formats
- Free format $FOUT decides fixed vs E using $FOTNV and $SIGD:
  - $FOTNV “brackets” the number so all printable digits lie in the integer part by multiplying/dividing by powers of 10, and returns the decimal exponent adjustment in AL (negative when there are fractional digits).
  - Very small magnitudes (e.g., < 0.01) count trailing zero significant digits via $SIGD to decide ‘E’ vs fixed.

- PRINT USING $PUFOT honors format flags in $FMTAX/$FMTCX (commas, asterisks fill, floating $, explicit ‘E’ format, digits left/right of the decimal point). When fixed format field is too small, $PUF falls back to free format and prefixes “%”.

3) Digit generation and rounding to the requested precision ($FOTCV)
- For single precision
  - Adds 0.5 at the rounding position (for the digit budget) and integerizes via $INT/$QINT.
  - Generates up to 7 digits using a table of powers of ten ($FOSTB), subtract‑and‑count to produce each digit, and inserts the decimal point/commas via $FOTED.

- For double precision
  - Converts the high‑precision integer part down in phases: 10 digits with double‑precision 10^n table ($FODTB), then 3 with single‑precision powers, then 3 with integer powers.
  - Before adding 0.5, checks for a value close to the cut‑off that would overflow on +0.5 via HIDBL guard. If so, divides by 10 and adjusts the decimal exponent.

- For both, when digits to the right are requested, internal rounding may repeatedly divide by 10 ($DIV10) to drop excess printable digits and keep the correct rounding point.

Tie‑handling
- For the digit generation paths that add 0.5 and integerize, the underlying $ROUNS/$ROUND logic enforces round‑half‑even at the mantissa level. The subtract‑and‑count per‑digit loops ($FOTCV) simply reflect the already rounded integer magnitude.

4) Zero insertion and separators
- Leading zeros (left of the decimal) via $FOTZ and $FOTZC (the latter keeps comma grouping in sync).
- Decimal point and commas handled by $FOTED using $FMTCX counters (CH: places before dp; CL: 3‑digit comma counter).
- Trailing zeros to the right via $FOTZ as dictated by the requested right‑side width.

5) Field fix‑up ($PUFXE)
- Applies fill rules (spaces/asterisks), floating currency, and optional trailing sign.

Edge cases to match
- Field overflow: fixed format too small → free format with leading “%”.
- Exactly halfway cases: must round to even. For double, watch the special path where the overflow byte becomes zero and the LSB is cleared to enforce evenness.
- Very small/large magnitudes: $FOTNV/$PUF apply extra 10^k to keep digit generation stable and avoid .5 adding overflow (HIDBL).

Practical porting notes (tests and contracts)

To get full PRINT/VAL parity, implement these contracts:

- Binary formats
  - MBF32/MBF64 as above (bias 128, sign in high mantissa bit). Zero iff exponent byte is 0.
  - On output, pack sign into the high mantissa bit; on input, extract sign from there.

- Normalization and rounding
  - Normalize mantissas left until top mantissa bit is 1; decrement exponent per shift.
  - Round to nearest, ties to even at the destination precision when packing to MBF32/MBF64.
  - For integer conversion, match −32768 treatment and overflows ($OVERR).

- VAL
  - Accumulate digits as 16‑bit integer, then SP, then DP as thresholds are crossed.
  - Track decimal places; apply explicit E±exp; scale by 10^k using exact powers or a faithfully rounded equivalent.
  - Respect %, !, # suffixes (and default types via DEFTBL).

- PRINT
  - Implement $FOTNV behavior (bracketing by powers of 10) so “free vs E” and fixed placement decisions match.
  - When converting to digits, add 0.5 at the correct place, then integerize and generate digits by subtractive division with the same digit budget (7 for SP, up to 16 for DP paths).
  - Apply comma grouping and decimal point placement using counters (not repeated division modulo 1000) to preserve exact comma timing.

Suggested minimal tests
- Round‑trip VAL(PRINT$) across boundaries and ties: 0.5, 1.5, 2.5, …; 0.05, 0.15, …; and negatives.
- Extremes: largest SP/DP values that still print as fixed without falling back; smallest non‑zero values that still print with fixed; transitions to E format.
- Suffix forcing: “1%”, “1!”, “1#”; literals with many digits that force the SP→DP transition.
- The −32768 integer rounding boundary, and rounding that increments the exponent.

Symbol map (handy references)

- Normalization
  - Single: $NORMS (jumps to $ROUNS)
  - Double: $NORMD (jumps to $ROUND)
- Rounding
  - Single: $ROUNS/$ROUNM (pack at PAKSP)
  - Double: $ROUND
- Input/VAL helpers
  - Digits: $FIDIG (int→SP→DP), exponent: $FINEX, force: $FINS/$FIND/$FINI, int paths: $CINC/$CIND
- Output/PRINT helpers
  - Bracket: $FOTNV, digit conversion: $FOTCV, dot/commas: $FOTED, zeros: $FOTZ/$FOTZC
  - Free format: $FOUT, PRINT USING: $PUFOT and $PUF

If you mirror these behaviors—including where and how rounding is applied—you’ll reproduce GW‑BASIC’s exact numeric I/O semantics.