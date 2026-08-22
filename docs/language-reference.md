# μλ Language Reference

## Intro

This language is a mix of Haskell and Python, you will come to see this in the following sections.

## Basics

μλ does **NOT** use any semi-colon or line ending delimiters (other than `\n`). In addition, it does **NOT** support for-loops (as it is a functional language). μλ also does **NOT** support pattern matching.

### Supported Data Types
    - Integers
    - Strings

*Note: list syntax (`[...]`) is recognised by the tokeniser but not yet accepted by the parser, so list literals cannot currently be used in a program.*

### Supported Keywords
    - fn (function)
    - ts (tail call signal)
    - ep (entry point)
    - return
    - if
    - else

### Supported Operators
    - +
    - -
    - *
    - /
    - >
    - <
    - =
    - ==
    - !=

### Comments

Line comments start with `//` and run to the end of the line:

```
// this whole line is ignored
x = 1 // so is this part of the line
```

### Supported Functions

    - print

*Note: this is currently the only built-in function registered with the interpreter. Things like `getLine`, `int`, and list operations are planned but not implemented yet — programs relying on them will fail at runtime.*

## Variables

Variables can be declared like in any other language: `var = 10` (this creates a new variable called `var` which is assigned the integer value `10`).

Variables are **IMMUTABLE** to maintain purity — re-assigning a name that already exists in the current scope is a runtime error.

## Entry Point

A program's entry point is marked with the `ep` keyword prepended to a function definition (regular or tail recursive). It must appear once, after every other top-level statement, and the function it marks must take no arguments:

```
ep fn main -> :
    ...
end
```

When the interpreter runs a program, it looks up the `ep`-marked function and calls it. If that function's body returns the function itself (i.e. `return main`), the entry point loops and calls it again instead of terminating — this is how a `main` function can repeat itself, since μλ has no other looping construct.

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

### Currying and Partial Application

Calling a function with fewer arguments than it declares does not fail — it returns a new function bound to the arguments supplied so far, waiting on the rest:

```
fn add -> a b:
    return a + b
end

add5 = add 5

ep fn main -> :
    return add5 10  // 15
end
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

Lambdas capture the environment they were created in, so closures work as expected:

```
fn makeAdder -> n:
    return (\x -> x + n)
end

ep fn main -> :
    add10 = makeAdder 10
    return add10 32  // 42
end
```

## Arithmetic

`*` and `/` bind tighter than `+` and `-`, following normal mathematical precedence:

```
x = 2 + 3 * 4  // 14, not 20
```

Division by zero is a runtime error.

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

A simple example program, tail-recursively computing a factorial and printing the result:

```
ts fn fact -> acc n:
    if n == 0:
        return acc
    else:
        return fact (acc * n) (n - 1)
    end
end

ep fn main -> :
    result = fact 1 5
    print result
    return result
end
```
