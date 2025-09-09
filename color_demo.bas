10 REM Color demonstration program
20 CLS
30 PRINT "GW-BASIC COLOR Statement Demonstration"
40 PRINT "======================================"
50 PRINT
60 REM Test foreground colors
70 PRINT "Foreground Colors (0-15):"
80 FOR I = 0 TO 15
90   COLOR I
100   PRINT "Color"; I; " ";
110 NEXT I
120 PRINT
130 PRINT
140 REM Test background colors
150 COLOR 15, 0: REM Reset to white on black
160 PRINT "Background Colors (0-7):"
170 FOR I = 0 TO 7
180   COLOR 15, I
190   PRINT "BG"; I; " ";
200 NEXT I
210 PRINT
220 PRINT
230 REM Some nice color combinations
240 COLOR 15, 0: REM Reset
250 PRINT "Nice Color Combinations:"
260 COLOR 14, 4: REM Yellow on red
270 PRINT "ALERT! ";
280 COLOR 0, 7: REM Black on light gray
290 PRINT "NORMAL ";
300 COLOR 15, 1: REM White on blue
310 PRINT "INFO ";
320 COLOR 10, 0: REM Light green on black
330 PRINT "SUCCESS ";
340 COLOR 12, 0: REM Light red on black
350 PRINT "ERROR"
360 PRINT
370 COLOR 15, 0: REM Reset to normal
380 PRINT "COLOR statement test complete!"
390 END
