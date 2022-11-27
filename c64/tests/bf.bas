   10 code$ = "$>,[>,]<.[<.]"
   20 for i= 1 to len(code$)
   30 poke -1 + i, asc(mid$(code$, i, 1))
   40 next
   50 print usr("hello")
