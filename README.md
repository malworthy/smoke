# Smoke 

A scripting language based on the clox interpreter from Bob Nystrom's excellent book Crafting Interpreters (https://craftinginterpreters.com/ )

Although the underlying code is based on clox, Smoke is a very different language.

## Compiling
To compile for linux you need the readline library.  To install it on Debian-based systems, run:
```
sudo apt-get install libreadline-dev
```
To build using gcc you can use the makefile in the src folder:
```
cd src
make
```
There is also a CMakeList.txt file included.


## Data types

There a 6 different data types
1) Numbers - numbers are double precision floating points
2) String
3) Boolean - either true or false
4) Lists - a list is a collection of values
5) Datetime
6) nil

```
var number = 0
var string = "hello"
var boolean = true
var list = [1,"hello", true]
var date = date.date("2022-02-01")
var something = nil
```

## Operators

|Operator|Description|
|--------|-----------|
()|Grouping
[]|Subscript
!|Not
*|Multiply
/|Divide
%|Modulo
+|Addition
-|Subtraction/Negate
==|Equals
!=|Not Equals
\>|Greater than
\>=|Greater than or equals
<|Less than
<=|Less than or equals
and|Logical and
or|Logical or
++|Increment by 1
--|Decrement by 1
+=|Addition assignment

## Variables

- Variables can be declared by using either var or const
- var is not permitted at a global level, you can only use const
- All variables must be initialized when declared

```
var x // not allowed
var x = 1 // all good (except at global scope)
const x = 1 // x will be read only
```

## Conditions: if then else

```
// single line if, use 'then'
if x == true then print "it's true"

// then not required if using block
if a == 1 and (b == 2 or c == 7) 
{
  print "do something
}
else
{
  print "something else"
}
```

## Looping

while
```
while a == 1 do print "doing stuff"

while a < 10
{
  print "still doing suff"
  a++
}
```

for
```
for x in [1..100] print x // prints 1-100

// use for to enumerate lists or strings
for x in [1,2,3] print x; 
for c in "this is a string" print c;
```

## Lists

```
const list = [];
list << "hello" // adds "hello" to the end of the list

var item = list[>>] // 'pop' item of end of list. 

// Slicing
const list [1,2,3,4,5,6];
print list[2:4]; // prints [3, 4]

// Ranges
print [1..5]; // prints [1, 2, 3, 4, 5]

// filtering a list
[1,2,3,4,5] where x => x >= 3 // returns [3, 4, 5]

// transforming a list
[1,2,3,4,5] select x => x * 10 // returns [10, 20, 30, 40, 50]

// updating elements
list[5] = "I've been updated!"

```

## Hash Tables

Hash tables are a set of key/pair values.  The key must be a string, while the value can be any valid expression, including functions, classes, lists or another hash table.
Converting a hash table to a string (via print or string interpolation) will produce valid JSON. 

```
// Create an empty hash table
const t = {}

// Create a hash table with values
const t = {"a" : 1, "b" : 2}

// Set a value
t["c"] = "test"

// Get a value
print t["a"]
```


## Functions
Functions are first class citizens.  They can be assigned to variables and passed in as parameters to functions.

Examples of usage:
```
// Declare a function
fn addNumbers(a,b) 
{ 
  return a + b; 
}

// Using 'arrow' notation
fn addNumbers(a,b) => a + b

// anonomous functions
const addNumbers = fn(a,b) => a + b

// calling a function
print addNumbers(1,1); // prints 2

// optional parameters
fn test(a,b,c=10) => a+b+c
print test(1,2) // prints 13
```


## String interpolation
Anything between '%{' and '}' is evaluated and embedded into the string.
You can also include a format string to format the evaluated value.  Format strings only work on numbers and dates. For other data types the format string will be ignored. Use a pipe '|' after the expression to add a format string.  The format string is anything between the '|' and the closing '}'.

See FormatStrings.md for details

```
var interpolated = "Value: %{1+1}" // "Value: 2"
var withFormatting = "%{100.1234|0.2f}" // "100.12"
```

## Classes

Examples of use:
```
// declaring a class
class Foo
{
  // Constructor
  init(name)
  {
    me.name = name
  }
  
  //Methods
  doSomething()
  {
    print "hello"
  }
  
  doSomethingElse()
  {
    print "goodbye"
    return 123
  }
}

// creating an instance
const foo = Foo("testing")

// using the instance
foo.doSometing()
print foo.name

foo.someProperty = 5 // adds property
print foo.doesNotExists // error as property does not exist yet

```


## Enums
You can define enums as below:
```
enum Animal
{
  Dog, // value = 0
  Cat, // value = 1
  Fish // value = 2
}

// To get the name of an enum value
Animal.name(Animal.Dog) //returns "Dog"
```


## Modules
Modules are a grouping of functions. See example below.
```
mod foo
{
  doSomething()
  {
    print "something"
  }

  doAnotherThing()
  {
    return 123
  }
}
```


## Using multiple files

You can split your code over multiple files.  

Example of how to import code from other files:

```
#inc codefile1.mal
#inc codefile2.mal

runFunctionInAnotherFile(123)
```


## Native functions

Console
- con.backcolor("blue") // changes the console background colour
- con.clear() // clears the screen
- con.input() // gets input from the console
- con.locate(x,y) // locates the cursor on the
- con.textcolor("red") // changes the console text colour 
- con.write("string") // write to the console without printing a newline afterwards (unlike print)
- con.getch() // gets the key in the keyboard buffer. Non-blocking.  If no key present it returns 0, else it will return an integer that correlates to the accii code, except for special keys like arrow keys.  Use Keys.name() to get the name of the key pressed.

Dates
- date.dartparts(date) // returns a list of all parts that make up the date ([year, month, day, hour, minute, second])
- date.now() // returns the current date/time in current timezone
- date.date(datestring) // creates a new date by converting datestring to a date.  Format for datestring is yyyy-mm-dd hh:MM:ss
- date.dateadd(date, interval, value) // adds/subtracts time interval from the date. Intervals: day, monthy,  year, hour, min, sec
- date.datediff(interval, startDate, endDate) // interval can be day, hour, minute, second

System
- sys.dir("path/to/folder") // returns a list of files
- sys.run(path)

String Functions
- string.ascii(string) // gets the ascii value of the first character of a string
- string.upper(string) // converts a string to upper case
- string.split(str, delim) // split a string on delimeter.  a returns list
- string.splitlines(str) // split a string on newline.  a returns list
- string.char(num) // converts an ascii value into a 1 character string
- string.trim(string)
- string.join(list) // makes a string by joining all elements of a list

File IO
- file.readlines(path) // reads a text file and returns a list of all the lines in file 
- file.open(filename, mode) // opens a file using mode ('r','w', etc). return reference to file (a number 0-255)
- file.write(fileref, text) // write to file opend by file.open
- file.close(fileref) // close file
- file.readchar(fileref) // reads 1 charater from a file. Returns nil if at end of file, or if can't read.

Utils
- args() // returns a list of command line arguments passed to the scripts
- clock() // number of seconds since program started
- len(list) // gets the length of a list or string
- num(string) // converts a string to a number
- rand(max) // gets a random number from 0 to max-1
- sleep(milliseconds) // suspend thread 
- type(variable) // gets the type of a variable.  Returns "Type" enum
  Types: Bool, Number, DateTime, String, Upvalue, Function, Native, Closure, List, Class, Instance, Method, Enum 

Math/Bitwise Operations
- math.bitand(value1, value2) // performs a bitwise and
- math.bitor(value1, value2) // bitwise or
- math.atan(value)
- math.cos(value)
- math.sin(value)
- math.tan(value)
- math.exp(value)
- math.log(value)
- math.sqrt(value)
- math.floor(value)
- math.ceil(value)



