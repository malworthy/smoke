# Commodore 64 Basic V2 Interpreter

## Overview

This is an interpreter based on the Commodore 64 Basic V2.  Most commands have been faithfully implemented to be compatible with the original 8 bit BASIC.  It's mostly commands that interacted with the Commodore 64 I/O that have been altered.

I wrote this project as a test for my own language smoke, to weed out bugs and to make sure it could at least do everything a commodore 64 could do.  It's just a fun project and not designed to be used to seriously develop software, but there's nothing stopping anyone from doing so!

## Data Types

My interpreter is a bit more lax with data types than the C64 was, and supports dynamic types.
- Variables ending in a $ are initialzed as strings, but it you want to you can store numbers too.
- Variables with no prefix are initialzed as zero, and are double precision floating points.
- Variables ending in a % are also initialzed as zero, and are double precision floating points, but will only ever be whole numbers.

Examples

```
a$ = "hello"    : rem a$ will be set to zero 
a$ = 1.1        : rem a$ will be set to zero
a = "hello"     : rem a will be set to zero
a = 1.1         : rem a will be set to 1.1
a = "2.2"       : rem a will be set to 2.2 (strings automatically converted to numbers)
a% = 1.1        : rem a% will be set to 1
a% = "hello"    : rem a% will be set to zero
a% = 1          : rem a% will be set to 1
```

## Language reference

All commands not listed here are fully compatible with the original Commodore 64 command.  The language is not case sensitive, so "Print", "PRINT" or "PrInT" will all do the same thing.

Like the commodore 64, only the first 2 characters of a variable are significant.  For example "count", "counter" and "co" all refer to the same variable.

Refer to the wiki here for an overview of all 71 Commodore 64 basic V2 commands: https://www.c64-wiki.com/wiki/BASIC#Overview_of_BASIC_Version_2.0_.28second_release.29_Commands


### CLR
This works in a similar way to the C64 version, but if executed during a program, program execution will stop.

### LOAD {filename}

This works very similar to the C64 Version, but the only argument it accepts is the full path and filename of the program you wish to load.  It will load the program into the shell, overwriting any existing loaded program.

### PEEK({memory address})

Reads the value of a "memory" address.  The "memory" in this interpreter is a list of 63356 elements.  Each element can contain a value from 0-255.  Valid memory addresses are from 0-63335.

### POKE {memory address}, {value}

Sets the value of a memory address.  This can be used to set the colors of the console, store bytes of data, or it can contain the code for a user defined function. Valid memory addresses are from 0-63335, while valid values are from 0-255.

The following memory addresses can be used to set the text colour and background colour of the console:

- 53281 - Sets the background colour
- 646 - Sets the text colour

### SAVE {filename}

Saves the current program to a file.

### STATUS/ST

### SYS {filename}

Runs a system command.  For example:

```
10 REM Opens Notepad on a windows system
20 SYS "notepad.exe"
```

### TAB({number})

This function will return a string with x number of tabs.

### TIME/TI

A system variable that contains the number of seconds since the script started running.

### TIME$/TI$

A system variable that contains the current time, in the format HH:MM:SS.

### USR

On the Commodore 64 this could be used to call a user-defined machine language routine, and return a resulting real number. In this version it does pretty much the same thing, expect instead of machine language, the user defined function is written in a version of Brainfuck.

The addresses 785–786 form a pointer to the routine.  Initially this will be set to zero and it is fine to store the rountine at address zero.

Example 1. Simple routine to add 1 to a number
```
10 REM Set location of routine to address 100
20 POKE 785, 0
30 POKE 786, 100
40 POKE 100, ASC(",") : REM store paramter is current cell
50 POKE 101, ASC("+") : REM add 1 to it's value
60 POKE 102, ASC(".") : REM return result
70 PRINT USR(10) : REM this will print 11
```


Example 2.  This creates a user defined function that will reverse a string.
```
05 REM User defined function to reverse a string
10 code$ = "$>,[>,]<.[<.]"
20 for i = 1 to len(code$)
30 poke i - 1, asc(mid$(code$, i, 1))
40 next
50 print usr("hello")
60 REM output will be "olleh"
```

### WAIT {milliseconds}

Suspend execution of a script for a specified number of milliseconds.



## C64Fuck (a variant of brainfuck)

This is a variation of brainfuck (https://en.wikipedia.org/wiki/Brainfuck).  It uses cells that contain double precision floating point numbers, as opposed to bytes.  It also contains commands to multiply and divide cells.  If the first character is a $ it signifies the result will be a string, else it will be a number. This is the language used by the USR command.

Overview of commands:

|Character|Meaning|
|---------|-----------|
|$|This is only valid if it is the first character of the function.  It present the output type will be a string, else output is a double precision floating point.|
|>|Move the data pointer to point to the next cell|
|<|Move the data pointer to point to the previous cell|
|+|Increment (increase by one) the byte at the data pointer. |
|-|Decrement (decrease by one) the byte at the data pointer. |
|*|Multiply the value in the previous cell by the value in the current cell, and store the result in the current cell|
|/|Divide the value in the previous cell by the value in the current cell, and store the result in the current cell|
|.|If output type is string: Add the character in the current cell to the result string.  The character based on the ascii value of the cell.  If the cell contains a number not in the range 0-255 this command is ignored.|
|.|If ouput type is number: Add the value of the current cell to the result|
|,|Read one byte from the input parameter.  If the parameter is a string, read 1 character at a time.  If the parameter is a number, read the actual number passed into the current cell.  If there are no parameters, or no more bytes to read from the parameter, the cell is left alone.|
|[|If the byte at the data pointer is zero, then instead of moving the instruction pointer forward to the next command, jump it forward to the command after the matching ] command. |
|]|If the byte at the data pointer is nonzero, then instead of moving the instruction pointer forward to the next command, jump it back to the command after the matching [ command. |






