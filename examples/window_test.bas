10 PRINT "Testing window resizing..."
20 SCREEN 0
30 PRINT "Text mode 720x400"
40 SCREEN 1  
50 PRINT "Should see 320x200 window"
60 SCREEN 2
70 PRINT "Should see 640x200 window"
80 SCREEN 12
90 PRINT "Should see 640x480 window"
100 SCREEN 0
110 PRINT "Back to 720x400 text mode"
120 END
