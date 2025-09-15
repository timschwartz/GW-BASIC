10 REM Test program for graphics GET and PUT operations
20 REM Create graphics patterns and copy them around
30 PRINT "Testing graphics GET and PUT operations"
40 SCREEN 1: REM Switch to graphics mode
50 REM Draw a simple pattern
60 FOR I = 0 TO 10
70   PSET(10+I,10+I),I MOD 16
80 NEXT I
90 REM Create a small circle
100 CIRCLE(50,50),15,14
110 REM Try to capture the pattern (simple test)
120 REM Compute array size for a 61x61 block: 4 header bytes + 3721 pixels
125 DIM PATTERN(4000)
130 GET(5,5)-(65,65),PATTERN
140 REM Put the pattern elsewhere
150 PUT(100,100),PATTERN
160 PRINT "Graphics GET/PUT test completed"
170 INPUT "Press ENTER to continue", DUMMY$
180 SCREEN 0: REM Back to text mode
190 END
