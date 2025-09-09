10 PRINT "Testing KEY trap functionality"
20 ON KEY(1) GOSUB 100
30 KEY(1) ON
40 PRINT "KEY(1) trap is now enabled"
50 PRINT "Press F1 to trigger the trap"
60 FOR I = 1 TO 10
70   PRINT "Waiting... "; I
80   FOR J = 1 TO 1000: NEXT J
90 NEXT I
95 END
100 PRINT "F1 KEY TRAP TRIGGERED!"
110 RETURN
