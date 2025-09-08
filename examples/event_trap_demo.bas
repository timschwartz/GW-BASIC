10 PRINT "Event Trap Demo"
20 PRINT "Press F1 to trigger key trap"
30 PRINT "Use Ctrl+C to trigger error trap"
40 PRINT
50 ON KEY(1) GOTO 1000
60 ON ERROR GOTO 2000
70 ON TIMER(3) GOTO 3000
80 KEY(1) ON
90 TIMER ON
100 PRINT "Main program running..."
110 FOR I = 1 TO 100
120   PRINT "Count: "; I
130   FOR J = 1 TO 1000: NEXT J  ' Simple delay loop
140 NEXT I
150 PRINT "Program ended normally"
160 END

1000 REM F1 Key trap handler
1010 PRINT "*** F1 Key pressed! ***"
1020 PRINT "Returning to main program..."
1030 RETURN

2000 REM Error trap handler  
2010 PRINT "*** Error trapped! ***"
2020 PRINT "Error code: "; ERR
2030 PRINT "Resume next..."
2040 RESUME NEXT

3000 REM Timer trap handler
3010 PRINT "*** Timer event! ***"
3020 PRINT "3 seconds elapsed"
3030 RETURN
