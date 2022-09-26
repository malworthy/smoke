# MAL (Mal's Amazing Language)

A scripting language based on the clox interpreter from Bob Nystrom's excellent book Crafting Interpreters (https://craftinginterpreters.com/ )

This is still a work in progress.  At the moment it supports most of lox features but not classes.
In addition to lox I've implemented lists and a small standard library. 


// Variables

My Rules: 
1) Global variables are not allowed.
2) All variables must be initialised

`var number = 0;`

`var string = "hello";`

`var boolean = true;`

`var list = [1,"hello", true];`

// Immutable variables (can be global)

`const str = "I cannot be changed";`

// if then else

`if x == true then print "it's true";`

`if a == 1 and (b == 2 or c == 7) then`
`{`
`  print "do something;`
`}`
`else`
`{`
`  print "something else";`
`}`

// looping

`while a == 1 do { print "doing stuff"; }`

// loop creates a variabled called 'i' that can be accessed in the scope of the loop

`loop 10 times print i;`

// use `for` to enumerate lists or strings

`for x in [1,2,3] print x;` 

`for c in "this is a string" print c;`

// Lists

// Lists can only be added to. (Hint: Use slicing to remove items from a list)

`const list = [];`

`add(list, "hello") // adds "hello" to the end of the list`

// Slicing

`const list [1,2,3,4,5,6];`

`print list[2:4]; // prints [3, 4]`

// Functions

`fn addNumbers(a,b) { return a + b; }`

`print addNumbers(1,1); // prints 2`

## Native functions

- add(list, item) // add item to list
- backColor("blue") // changes the console background colour
- clear() // clears the screen
- clock() // number of seconds since program started
- dir("path/to/folder") // returns a list of files (wip)
- input() // gets input from the console
- locate(x,y) // locates the cursor on the console
- textColor("red") // changes the console text colour 
- write("string") // write to the console without printing a newline afterwards (unlike print)

