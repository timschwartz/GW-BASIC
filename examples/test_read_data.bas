10 REM Test program for READ/DATA/RESTORE functionality
20 DATA 42, "Hello World", 3.14159, -100
30 DATA "Test String", 999, 2.718, "End"
40 REM Read some values
50 READ A, B$, C, D
60 PRINT "A =", A
70 PRINT "B$ =", B$
80 PRINT "C =", C
90 PRINT "D =", D
100 REM Read more values
110 READ E$, F, G, H$
120 PRINT "E$ =", E$
130 PRINT "F =", F
140 PRINT "G =", G
150 PRINT "H$ =", H$
160 REM Test RESTORE
170 RESTORE
180 READ FIRST
190 PRINT "After RESTORE, first value =", FIRST
200 END
