10 REM ON GOTO Examples - Comprehensive Test
20 PRINT "ON GOTO and ON GOSUB Examples"
30 PRINT "============================="
40 PRINT
50 REM Test 1: Basic ON GOTO with different indices
60 PRINT "Test 1: ON GOTO with indices 0, 1, 2, 3, 4"
70 FOR I = 0 TO 4
80   PRINT "ON"; I; "GOTO 200,300,400"
90   ON I GOTO 200,300,400
100   PRINT "  -> No jump (index"; I; "out of range)"
110   GOTO 130
120 REM Jump targets for Test 1
200 PRINT "  -> Jumped to line 200 (index 1)"
210 GOTO 130
300 PRINT "  -> Jumped to line 300 (index 2)"
310 GOTO 130
400 PRINT "  -> Jumped to line 400 (index 3)"
410 GOTO 130
130 NEXT I
140 PRINT
150 REM Test 2: ON GOSUB demonstration
160 PRINT "Test 2: ON GOSUB with index 2"
170 PRINT "Calling: ON 2 GOSUB 500,600,700"
180 ON 2 GOSUB 500,600,700
190 PRINT "  -> Returned from subroutine"
420 PRINT
430 REM Test 3: Using variables
440 PRINT "Test 3: ON with variables"
450 A = 1: B = 2: C = 3
460 PRINT "A="; A; ", B="; B; ", C="; C
470 PRINT "ON B GOTO 800,850,900"
480 ON B GOTO 800,850,900
490 PRINT "Should not reach this line"
495 GOTO 950
500 REM Subroutines for Test 2
510 PRINT "  Subroutine 500 called"
520 RETURN
600 PRINT "  Subroutine 600 called"
610 RETURN
700 PRINT "  Subroutine 700 called"
710 RETURN
800 REM Targets for Test 3
810 PRINT "  -> Jumped to line 800 (B=1)"
820 GOTO 950
850 PRINT "  -> Jumped to line 850 (B=2)"
860 GOTO 950
900 PRINT "  -> Jumped to line 900 (B=3)"
910 GOTO 950
950 PRINT
960 PRINT "All ON GOTO tests completed successfully!"
970 END
