# MAL (Mal's Amazing Language)

A scripting language based on the clox interpreter from Bob Nystrom's excellent book Crafting Interpreters (https://craftinginterpreters.com/ )

Although the underlying code is based on clox, MAL is a very different language.

## Compiling
for a better repl experience on linux install the readline library

sudo apt-get install libreadline-dev

## Data types

There a 5 different data types
1) Numbers - numbers are double precision floating points
2) String
3) Boolean - either true or false
4) Lists - a list is a collection of values
5) Datetime

```
var number = 0
var string = "hello"
var boolean = true
var list = [1,"hello", true]
var date = date("2022-02-01")
```

## Variables

- Variables can be declared by using either var or const
- var is not permitted at a global level, you can only use const
- All variables must be initialized when declared

```
var x // not allowed
var x = 1 // all good
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
- backColor("blue") // changes the console background colour
- clear() // clears the screen
- input() // gets input from the console
- locate(x,y) // locates the cursor on the
- textColor("red") // changes the console text colour 
- write("string") // write to the console without printing a newline afterwards (unlike print)
- getch() // gets the key in the keyboard buffer. Non-blocking.  If no key is present returns an empty string.

Dates
- dartparts(date) // returns a list of all parts that make up the date ([year, month, day, hour, minute, second])
- now() // returns the current date/time in current timezone
- date(datestring) // creates a new date by converting datestring to a date.  Format for datestring is yyyy-mm-dd hh:MM:ss
- dateadd(date, interval, value) // adds/subtracts time interval from the date. Intervals: day, monthy,  year, hour, min, sec
- datediff(interval, startDate, endDate) // interval can be day, hour, minute, second

System
- dir("path/to/folder") // returns a list of files (wip - currently broken :( )
- run(path)

String Functions
- string.ascii(string) // gets the ascii value of the first character of a string
- string.upper(string) // converts a string to upper case
- string.split(str, delim) // split a string on delimeter.  a returns list
- string.splitlines(str) // split a string on newline.  a returns list
- string.char(num) // converts an ascii value into a 1 character string

File IO
- file.readlines(path) // reads a text file and returns a list of all the lines in file 
- file.open(filename, mode) // opens a file using mode ('r','w', etc). return reference to file (a number 0-255)
- file.write(fileref, text) // write to file opend by file.open
- file.close(fileref) // close file

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



