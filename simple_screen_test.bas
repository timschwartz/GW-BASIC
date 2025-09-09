10 PRINT "Testing SCREEN modes:"
20 SCREEN 0
30 PRINT "Text mode set"
35 PRINT "Press any key for next mode..."
36 INPUT A$
40 SCREEN 1  
50 PRINT "CGA 320x200 mode set"
55 PRINT "Press any key for next mode..."
56 INPUT A$
60 SCREEN 12
70 PRINT "VGA 640x480 mode set"
75 PRINT "Press any key for next mode..."
76 INPUT A$
80 SCREEN 0
90 PRINT "Back to text mode"
100 END
