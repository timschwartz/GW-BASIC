10 REM DRAW Statement Demonstration
20 SCREEN 1  
30 CLS
40 REM Draw a simple house
50 DRAW "M160,120"        : REM Move to starting position
60 DRAW "U40R60D40L60"    : REM Draw house outline
70 DRAW "M160,120U40R30D20L30" : REM Draw roof
80 REM Draw a door
90 DRAW "M170,120L5U15R5D15"
100 REM Draw windows
110 DRAW "M150,105L8U8R8D8"    : REM Left window
120 DRAW "M200,105L8U8R8D8"    : REM Right window
130 REM Draw a tree using scaling and rotation
140 DRAW "M100,120"       : REM Move to tree position
150 DRAW "S2C2U20"        : REM Scale 2x, color 2, draw trunk
160 DRAW "M100,100"       : REM Move to top of trunk
170 DRAW "S1C1"           : REM Reset scale, color 1 for leaves
180 DRAW "A0U10E5F5D10G5H5" : REM Draw tree crown
190 REM Draw a sun in the corner
200 DRAW "M20,20"         : REM Move to corner
210 DRAW "C3"             : REM Color 3 (bright)
220 FOR I=0 TO 7
230   DRAW "A" + STR$(I) + "U5BU2D7" : REM Rays at different angles
240 NEXT I
250 PRINT "DRAW statement demo complete!"
260 PRINT "Press any key to continue..."
270 A$ = INPUT$(1)