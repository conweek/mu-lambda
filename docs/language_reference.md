# μλ Language Reference

## Intro

This language is a mix of Haskell and Python, you will come to see this in the following sections.

## Basics

μλ does **NOT** use any semi-colon or line ending delimiters (other than `\n`). In addition, it does **NOT** support for-loops (as it is a functional language). μλ also does **NOT** support pattern matching.

### Supported Data Types:
    - Integers
    - Strings
    - Lists (of integers and strings)

### Supported Keywords:
    - fn (function)
    - ts (tail call signal)
    - return
    - if
    - else

### Supported Operators
    - +
    - -
    - >
    - <
    - =
    - ==
    - !=

### Supported Functions
    - print
    - getLine

## Variables

Variables can be declared like in any other language: `var = 10` (this creates a new variable called `var` which is assigned the integer value `10`).

Variables are **IMMUTABLE** to maintain purity.

## Functions

### Regular Functions

Standard functions are declared as such:

```
fn name -> arg1 arg2 arg3:
    ...
    return ...
end
```

*Note: you can omit the arguments if you do not take any in*

```
fn name -> :
    ...
    return ...
end
```

Functions must always return *something*.

To call a function, you use the following Haskell-like notation *(noting args could be omitted if there is no arguments or multiple arguments provided)*:

```
result = name args
```

### Tail Recursive Functions

Tail recursive functions are declared similarly to regular functions, however they have the tail call signal prepended to their function prototype. This signal is merely a promise you make to the interpreter that the function is in fact tail recursive. Misusing the signal will likely result in program death.

```
ts fn name -> args:
    ...
    return ...
end
```

### Lambda Functions

Lambda functions are written similarly to Haskell:

```
\x -> x + 1
```

They can also be assigned as variables:

```
succ = (\x -> x + 1)
```

and used as such:

```
result = succ 1
```

## If Statements

### If

If statements can be constructed by:

```
if condition:
    ...
end
```

### Else

Must be adjoined to an `if` statement, cannot be on its own.

```
if condition:
    ...
else:
    ...
end
```

## Example program

A simple non-pure example program can be found here:

```
// Tail recursively calculates factorial
ts fn fact -> ans curr:
    
    if curr == 1:
        return ans
    end

    return (fact (ans * curr) (curr - 1))

// Recursively polls for input
ts fn handle_input -> currInput:
    
    if currInput != "":
        return (int (currInput)) 

    return (handle_input getLine)

// This is not a purely tail recursive function, but may be fine?
ts fn main -> :
    print "Please enter a number: "
    input = handle_input ""

    result = fact input

    print "Generated following result:"
    print result

    return main
end

```
