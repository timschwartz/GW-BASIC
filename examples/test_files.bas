10 REM Test FILES command with patterns
20 REM Create some test files
30 OPEN "file_a.txt" FOR OUTPUT AS #1: PRINT #1, "A" : CLOSE #1
40 OPEN "file_b.txt" FOR OUTPUT AS #1: PRINT #1, "B" : CLOSE #1
50 OPEN "note_a.log" FOR OUTPUT AS #1: PRINT #1, "LOG" : CLOSE #1
60 PRINT "Listing all .txt files:"
70 FILES "*.txt"
80 PRINT "Listing files starting with file_*:"
90 FILES "file_*.*"
100 PRINT "Listing all files:"
110 FILES
120 REM Cleanup created files
130 KILL "file_a.txt"
140 KILL "file_b.txt"
150 KILL "note_a.log"
160 PRINT "FILES test done"
170 END
