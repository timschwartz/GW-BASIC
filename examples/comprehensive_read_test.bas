10 REM Comprehensive READ/DATA/RESTORE test
20 DATA 42, "Hello", 3.14159, -100, "World", 999
30 DATA 2.718, "Test123", 0, "Final"
40 PRINT "=== Testing READ/DATA functionality ==="
50 READ NUM1, STR1$, PI, NUM2
60 PRINT "First batch:"
70 PRINT "  Number:", NUM1
80 PRINT "  String:", STR1$
90 PRINT "  Pi:", PI
100 PRINT "  Negative:", NUM2
110 READ STR2$, BIG
120 PRINT "Second batch:"
130 PRINT "  String2:", STR2$
140 PRINT "  Big number:", BIG
150 READ E, STR3$, ZERO, LAST$
160 PRINT "Third batch:"
170 PRINT "  E:", E
180 PRINT "  String3:", STR3$
190 PRINT "  Zero:", ZERO
200 PRINT "  Last string:", LAST$
210 PRINT
220 PRINT "=== Testing RESTORE functionality ==="
230 RESTORE
240 READ FIRST, SECOND$
250 PRINT "After RESTORE:"
260 PRINT "  First value:", FIRST
270 PRINT "  Second value:", SECOND$
280 END
