100 DEF FN SG(X)=X/(ABS(X)-(X=0))
200 FOR I=-10 TO 10 STEP 10
210 PRINT I,SGN(I),FN SG(I)
220 NEXT