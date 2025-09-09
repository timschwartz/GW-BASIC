10 PRINT "Testing SCREEN statement modes"
20 PRINT "Mode 0: Text 80x25"
30 SCREEN 0
40 PRINT "Now in text mode"
50 PRINT "Press any key for next mode..."
60 INPUT A$
70 PRINT "Mode 1: CGA 320x200"
80 SCREEN 1
90 PRINT "Now in CGA graphics mode"
100 INPUT A$
110 PRINT "Mode 2: CGA 640x200"
120 SCREEN 2
130 PRINT "Now in higher resolution CGA"
140 INPUT A$
150 PRINT "Mode 12: VGA 640x480"
160 SCREEN 12
170 PRINT "Now in VGA graphics mode"
180 INPUT A$
190 PRINT "Back to text mode"
200 SCREEN 0
210 PRINT "Test complete!"
