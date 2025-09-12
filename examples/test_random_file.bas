10 REM Test random file I/O with GET and PUT
20 OPEN "test.dat" FOR RANDOM AS #1 LEN = 32
30 FIELD #1, 10 AS NAME$, 5 AS AGE$, 17 AS ADDRESS$
40 LSET NAME$ = "John Doe"
50 LSET AGE$ = "25"
60 LSET ADDRESS$ = "123 Main St"
70 PUT #1, 1
80 LSET NAME$ = "Jane Smith"
90 LSET AGE$ = "30"
100 LSET ADDRESS$ = "456 Oak Ave"
110 PUT #1, 2
120 REM Now read the records back
130 GET #1, 1
140 PRINT "Record 1: " + NAME$ + ", " + AGE$ + ", " + ADDRESS$
150 GET #1, 2
160 PRINT "Record 2: " + NAME$ + ", " + AGE$ + ", " + ADDRESS$
170 CLOSE #1
180 END
