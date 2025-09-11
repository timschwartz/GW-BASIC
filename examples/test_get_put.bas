10 REM Test program for GET and PUT file operations
20 REM Create a random access file and test record operations
30 PRINT "Testing GET and PUT for random access files"
40 PRINT
50 REM Open a random access file with 64-byte records
60 OPEN "testfile.dat" FOR RANDOM AS #1 LEN=64
70 REM Define field layout: 20-char name, 10-char phone, 4-byte age
80 FIELD #1, 20 AS NAME$, 10 AS PHONE$, 4 AS AGE$
90 REM Write first record
100 LSET NAME$ = "John Doe"
110 LSET PHONE$ = "555-1234"
120 LSET AGE$ = "25"
130 PUT #1, 1
140 REM Write second record 
150 LSET NAME$ = "Jane Smith"
160 LSET PHONE$ = "555-5678"
170 LSET AGE$ = "30"
180 PUT #1, 2
190 REM Read back first record
200 GET #1, 1
210 PRINT "Record 1:"
220 PRINT "Name: "; NAME$
230 PRINT "Phone: "; PHONE$
240 PRINT "Age: "; AGE$
250 PRINT
260 REM Read back second record
270 GET #1, 2
280 PRINT "Record 2:"
290 PRINT "Name: "; NAME$
300 PRINT "Phone: "; PHONE$
310 PRINT "Age: "; AGE$
320 CLOSE #1
330 PRINT "Test completed successfully!"
340 END
