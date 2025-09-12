10 REM Test KILL deletes files
20 OPEN "kill_test.tmp" FOR OUTPUT AS #1: PRINT #1, "temp" : CLOSE #1
30 PRINT "Before KILL, .tmp files:"
40 FILES "*.tmp"
50 KILL "kill_test.tmp"
60 PRINT "After KILL, .tmp files:"
70 FILES "*.tmp"
80 PRINT "KILL test done"
90 END
