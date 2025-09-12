10 REM Test NAME (rename) files
20 OPEN "oldname.tmp" FOR OUTPUT AS #1: PRINT #1, "x" : CLOSE #1
30 PRINT "Before rename, .tmp files:"
40 FILES "*.tmp"
50 NAME "oldname.tmp" AS "newname.tmp"
60 PRINT "After rename, .tmp files:"
70 FILES "*.tmp"
80 REM Clean up
90 KILL "newname.tmp"
100 PRINT "NAME test done"
110 END
