# μλ Language Reference

## Intro

This language is a mix of Haskell and Python, you will come to see this in the following sections.

## Basics

μλ does **NOT** use any semi-colon or line ending delimiters (other than `\n`). In addition, it does **NOT** support for-loops (as it is a functional language). μλ also does **NOT** support pattern matching.

### Supported Data Types
    - Integers
    - Strings

*Note: there are no lists. `[` and `]` are not recognised by the tokeniser at all, so a list
literal is a tokenising error rather than a parse error.*

The tokeniser only knows the characters used by the operators, literals and punctuation described
below. Anything else, including `[` `]` `#` `@` `?` `$` `;` `,` and `.`, is unsupported, and a
program containing one will not run.

### Supported Keywords
    - fn (function)
    - ts (tail call signal)
    - ep (entry point)
    - return
    - if
    - else

### Supported Operators

Arithmetic: `+` `-` `*` `/` `%`

*Note: integer division truncates toward zero, and `%` takes the sign of the left operand, so
`-17 % 5` is `-2`. Both `/` and `%` by zero are runtime errors.*

Comparison, each yielding `1` or `0`: `>` `>=` `<` `<=` `==` `!=`

Bitwise: `&` `|` `^` `<<` `>>` and unary `~`

Assignment: `=`

Because a comparison evaluates to `1` or `0`, `&` and `|` double as logical and/or. Parenthesise
each comparison:

```
alive = (cur == 1) & ((n == 2) | (n == 3))
```

*Note: `~` is only accepted before the first operand of a bitwise chain, so `a & ~b` is a parse
error. Write `a & (~b)` or `~b & a`.*

### Comments

Line comments start with `//` and run to the end of the line:

```
// this whole line is ignored
x = 1 // so is this part of the line
```

### String Escapes

Inside a string literal, `\n` `\t` `\r` `\e` `\\` and `\"` are decoded, where `\e` is the escape
character. An unrecognised escape yields the bare character.

This is the only way to get control characters into a program, because the reader discards any
byte outside printable ASCII. ANSI sequences must therefore be written as escapes:

```
print "\e[2J"        // clear the screen
write "\e[38;5;46m"  // set a colour, no newline
```

### Supported Functions

| Call | Arguments | Behaviour |
|---|---|---|
| `print x` | int or string | writes it, followed by a newline |
| `write x` | int or string | writes it with no newline |
| `sleep n` | int | blocks for `n` milliseconds |
| `halt x` | anything, ignored | stops the program immediately |
| `reset x` | anything, ignored | discards every definition and frees all memory |
| `gpioSet dev pin val` | string, int, int | drives `pin` on device `dev` |
| `gpioRead dev pin` | string, int | returns the level of `pin` |
| `i2cRegWrite dev addr reg val` | string, int, int, int | writes a device register |
| `i2cRegRead dev addr reg` | string, int, int | reads a device register, returns 0..255 |

These are the only built-ins registered with the interpreter. Anything else, such as `getLine`,
`int`, or list operations, does not exist and will fail at runtime.

Hardware built-ins curry like ordinary functions, so a device and address can be bound once and
reused:

```
rd = i2cRegRead "i2c1" 29
x = rd 1
y = rd 3
```

`dev` is looked up by name, so the node needs a `label` property in the board overlay, for example
`&i2c1 { label = "i2c1"; status = "okay"; };`.

*Note: `i2cRegRead` returns an unsigned byte. A register the sensor means as signed reads as
128..255 where -128..-1 is expected, so convert it:*

```
fn sgn -> v:
    if v > 127:
        return v - 256
    end
    return v
end
```

## Variables

Variables can be declared like in any other language: `var = 10` (this creates a new variable called `var` which is assigned the integer value `10`).

Variables are **IMMUTABLE** to maintain purity. Re-assigning a name that already exists in *any*
enclosing scope, not merely the current one, is a runtime error. Function parameters are the
exception, since they bind fresh on each call rather than through `=`.

*Note: `fn` does not perform this check. Re-declaring an existing function is silently ignored and
the original definition keeps winning, because lookup returns the first match. Use `reset 0` to
clear a session and start over.*

## Entry Point

A program's entry point is marked with the `ep` keyword prepended to a function definition (regular or tail recursive). It must appear once, after every other top-level statement, and the function it marks must take no arguments:

```
ep fn main -> :
    ...
end
```

When the interpreter runs a program, it looks up the `ep`-marked function and calls it. If that function's body returns the function itself (i.e. `return main`), the entry point loops and calls it again instead of terminating.

That gives two ways to loop. `return main` repeats the entry point but carries nothing between passes, since it takes no arguments. To keep state, tail-call a `ts` helper instead and thread the state through its parameters:

```
ts fn count -> n:
    print n
    sleep 500
    return count (n + 1)
end

ep fn main -> :
    return count 1
end
```

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

The promise applies only to the recursive call, which must be the expression that is `return`ed.
Everything else in the body is ordinary, so calls made for their side effects are fine:

```
ts fn count -> n:
    print n          // an ordinary call, not a tail call
    sleep 200
    if n == 0:
        return 0
    end
    return count (n - 1)
end
```

A `ts` loop runs in constant stack and constant memory, so it can repeat indefinitely.

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

Bitwise operators bind tighter still, which is the **opposite** of C. Parenthesise when mixing:

```
a = 1 & 3 + 4   // (1 & 3) + 4  = 5
b = 2 * 3 & 1   // 2 * (3 & 1)  = 2
c = 1 << 3 + 1  // (1 << 3) + 1 = 9
```

Comparisons sit loosest of all and do not chain, so `a == b == c` is invalid.

Function application binds tightest but only takes atomic arguments, so `f x + 1` means `(f x) + 1`.
For the same reason a bare negative literal reads as a subtraction: `f -5` is `f - 5`, and the
argument must be written `f (-5)`.

`%` sits at the same level as `*` and `/`, so `17 % 5 + 1` is `(17 % 5) + 1`, and operators of
equal precedence run left to right: `2 * 7 % 4` is `(2 * 7) % 4`.

Division or modulo by zero is a runtime error. Integers are 32-bit and signed.

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

## Returning More Than One Value

A function returns a single value, and there are no lists, so several values have to be packed
into one integer with shifts and unpacked by the caller:

```
// pack a seed and a coordinate pair
return (seed << 6) | (y << 3) | x
```

```
// unpack them again
seed = r >> 6
x    = r & 7
y    = (r >> 3) & 7
```

The same trick carries state through a tail-recursive loop, which is the only way to keep state
across iterations given that variables are immutable.

## Stopping a Program

`halt 0` ends a running program immediately. Pressing ctrl-c does the same thing from the
keyboard, and both unwind the same way: the whole submission is rolled back, so every definition
it made and every byte it allocated are released. A program that ends this way leaves nothing
behind.

`reset 0` goes further and clears the entire session, discarding all definitions made so far. It
is the only way to redefine an existing function, since `fn` will not overwrite one.

Built-ins take at least one argument because a call with no arguments is just a variable
reference, so `halt` and `reset` are written with a dummy argument that they ignore.

## Limits

A program is submitted to the REPL in one go, and the buffers are fixed:

- **4096 bytes** for the whole program, comments included
- **128 bytes** per line

Exceeding the first is reported as `too long`. Comments count, so keep them lean in a large
program.

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
