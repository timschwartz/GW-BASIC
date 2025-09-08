10 PRINT "Testing FOR-NEXT loops"
20 PRINT "Test 1: Simple counting loop"
30 FOR I = 1 TO 3
40 PRINT "I ="; I
50 NEXT I
60 PRINT "Test 2: Loop with step"
70 FOR J = 10 TO 20 STEP 5
80 PRINT "J ="; J
90 NEXT J
100 PRINT "Test 3: Nested loops"
110 FOR X = 1 TO 2
120 FOR Y = 1 TO 2
130 PRINT "X ="; X; " Y ="; Y
140 NEXT Y
150 NEXT X
160 PRINT "All tests completed!"
170 END
