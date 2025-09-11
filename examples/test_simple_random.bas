10 REM Simple test for random access file
20 PRINT "Testing OPEN for RANDOM mode"
30 OPEN "simple.dat" FOR RANDOM AS #1 LEN=64
40 PRINT "File opened successfully"
50 CLOSE #1
60 PRINT "File closed successfully"
70 END
