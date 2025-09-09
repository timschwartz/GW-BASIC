10 REM Test CHR$ and STR$ functions
20 PRINT "Testing CHR$ function:"
30 PRINT "CHR$(65) = "; CHR$(65)
40 PRINT "CHR$(97) = "; CHR$(97)
50 PRINT "CHR$(48) = "; CHR$(48)
60 PRINT
70 PRINT "Testing STR$ function:"
80 PRINT "STR$(123) = "; STR$(123)
90 PRINT "STR$(-45) = "; STR$(-45)
100 PRINT "STR$(3.14) = "; STR$(3.14)
110 PRINT
120 PRINT "Combined test:"
130 FOR I = 65 TO 70
140   PRINT "ASCII "; STR$(I); " = "; CHR$(I)
150 NEXT I
160 END
