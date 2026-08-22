---
name: mu-lambda
description: Use when the user asks to write, generate, review, or fix a program in μλ (mu-lambda) — the small functional language implemented by this repo's repl/ (tokeniser.c, parser.c, interpreter.c). Trigger on requests like "write a mu-lambda program that...", "give me a μλ function for...", "why won't this μλ program parse", or work touching examples/*.mu, docs/rules.md, or docs/language-reference.md.
---

# μλ (Mu-Lambda)

A small, purely functional, Haskell/Python-flavored language, tokenised/parsed/interpreted by
`repl/src/{tokeniser,parser,interpreter}.c` and running on Zephyr.

**The C source is the only ground truth.** `docs/rules.md` and `docs/language-reference.md` are
now substantially out of date — they document neither the bitwise operators, the string escapes,
nor any builtin beyond the original four. Do not trust them; read
`repl/src/{tokeniser,parser,interpreter,builtins}.c` and `repl/src/main.c` when in doubt. Working,
tested programs live in `examples/`.

## Core rules

- No semicolons or statement terminators other than `\n`.
- No `for`/`while` loops. The only repetition is recursion, plus an entry point returning itself.
- No pattern matching, no lists, no error handling. A builtin that fails aborts the whole program.
- Two data types only: **integers** (32-bit signed) and **strings**. There are no lists: `[` and
  `]` are not recognised by the tokeniser at all, so they are a tokenising error, not a parse
  error. (`VAR_LIST` still sits in the `value_type_t` enum but nothing ever produces it.)
- The tokeniser recognises only the characters listed under Operators, plus `( ) " \ : ->` and
  identifier/number characters. **Any other character, e.g. `[ ] # @ ? $ ; , .`, wedges the REPL** —
  see Gotchas.
- Variables are **immutable**: `name = expr` fails at runtime if `name` is already bound
  **anywhere in an enclosing scope**. Function parameters are the exception (fresh per call).
- Comments: `// rest of line`. Blocks close with `end`; indentation is cosmetic.

## Keywords, operators, literals

| Keyword | Meaning |
|---|---|
| `fn` | function definition |
| `ts` | tail-call signal, prefixes `fn` |
| `ep` | marks the program's entry-point function |
| `return` / `if` / `else` / `end` | return, conditional, block close |

| Symbol | Meaning |
|---|---|
| `+ - * / %` | arithmetic (ints only). `/` truncates toward zero, `%` takes the sign of the left operand (`-17 % 5` is `-2`); either by zero is a runtime error |
| `> < >= <= == !=` | comparison, yields 1 or 0 |
| `& \| ^ << >> ~` | bitwise and, or, xor, shifts, complement |
| `=` | assignment (first binding only) |
| `->` | separates a function name from params, and lambda params from body |
| `:` | opens a block | 
| `\` | starts a lambda: `(\x -> x + 1)` |
| `( )` `"..."` `123` | grouping, string literal, integer literal |

Because comparisons yield 1/0, `&` and `|` double as logical and/or:
`((cur==1) & ((n==2) | (n==3)))`. Parenthesise each comparison.

### String escapes

Decoded in `convert_value` (interpreter.c): `\n` `\t` `\r` `\e` (ESC, 0x1b) `\\` `\"`. An unknown
escape yields the bare character. This is the **only** way to get control characters into a
program: `mu_readline` drops every byte outside printable ASCII and swallows a raw ESC, so ANSI
sequences must be written as `"\e[2J"`, never as a literal escape byte.

## Built-ins

Nine are registered in `repl/src/main.c`. Anything else does not exist — do not call it. A builtin
given the wrong type is a hard runtime error that aborts evaluation.

| Call | Signature | Behavior |
|---|---|---|
| `print x` | int or string | writes it **plus a newline** |
| `write x` | int or string | writes it with **no newline** — needed for colour codes and partial lines |
| `sleep n` | int | blocks `n` ms |
| `halt x` | any (ignored) | stops the program immediately; unwinds like ctrl-c so the caller rolls the submission back |
| `reset x` | any (ignored) | throws away every definition and clears the arena |
| `gpioSet dev pin val` | string, int, int | configures `pin` `OUTPUT \| PULL_UP`, sets it |
| `gpioRead dev pin` | string, int | configures `pin` `INPUT`, returns its level |
| `i2cRegWrite dev addr reg val` | string, int, int, int | writes `val` to register `reg` |
| `i2cRegRead dev addr reg` | string, int, int | reads register `reg`, returns **unsigned 0..255** |

All hardware builtins curry like user functions — `i2cRegRead "i2c1" 29` is a reusable reader.

`print`/`write` go straight to the console, **not** through `printk`, because printk prefixes every
call with `ESC[0;39m` which would wipe out any colour the program set.

`dev` is resolved by `device_get_binding`, so the node needs a **`label` property** in the board
overlay (`repl/boards/<board>.overlay`), e.g. `&i2c1 { label = "i2c1"; status = "okay"; };`. No such
labelled node exists on `native_sim`, so hardware programs only run on real boards.

**`i2cRegRead` returns unsigned.** Sensor registers that are signed two's-complement (e.g. MMA8452
`OUT_X_MSB`) read as 128..255 where you expect -128..-1. Convert:
`fn sgn -> v: if v > 127: return v - 256 end return v end`

## Grammar (as implemented by repl/src/parser.c)

```
program      = { statement NEWLINE } [ EP ( tsFn | fn ) NEWLINE ] EOF
block        = { statement NEWLINE } END
statement    = fn | tsFn | ifStmt | RETURN exprStatement
             | IDENTIFIER ASSIGNMENT exprStatement          // x = expr
             | call                                         // bare call as a statement
fn           = FN IDENTIFIER ARROW { IDENTIFIER } COLON NEWLINE block
tsFn         = TS fn
ifStmt       = IF exprStatement COLON NEWLINE block-body
               [ ELSE COLON NEWLINE block-body ] END        // shares END with the if
exprStatement= comparison
comparison   = (lambda | term) [ (== | != | > | < | >= | <=) (lambda | term) ]  // NOT chainable
lambda       = "(" "\" IDENTIFIER { IDENTIFIER } "->" exprStatement ")" { atomic }
term         = [ "-" ] factor { ("+" | "-") factor }
factor       = bitwise { ("*" | "/" | "%") bitwise }
bitwise      = [ "~" ] (call | atomic) { ("&" | "|" | "^" | "<<" | ">>") (call | atomic) }
call         = IDENTIFIER { atomic | lambda }               // curried application
atomic       = INT | STR | IDENTIFIER | "(" exprStatement ")"
```

Precedence, high to low: bitwise > `* / %` > `+ -` (with unary `-`) > comparison (single level, no
chaining). Application binds tighter than `+ -`: `f x + 1` is `(f x) + 1`.

Note bitwise binds **tighter** than arithmetic, the opposite of C. `a & b + c` is `(a & b) + c`.
Parenthesise when mixing.

### Gotchas

- **An unrecognised character hangs the REPL, it does not just error.** `tokenise()` returns
  `TOKEN_ERR` *without advancing the cursor*, and `get_token_list()` neither bounds its write
  index nor stops on `TOKEN_ERR`, so it fills the token array and keeps writing past it. A single
  stray `[` or `#` floods the console with `unexpected token 0` and the session never comes back.
  Verified: a submission after one never runs.
- **`~` only works in leading position.** `bitwise` accepts `~` before its *first* operand only, so
  `a & ~b` is a parse error. Write `a & (~b)` (parens make it an atomic) or `~b & a`.
- **A bare `-N` cannot be a call argument.** `f -5` parses as `f - 5`. Write `f (-5)`. In a
  comparison it is fine, since the right side is a `term`: `x < -20` parses.
- **Zero-parameter functions cannot be called.** `call` with no arguments is just a variable
  reference, which evaluates to the closure itself. Give every helper a dummy parameter and call it
  as `btn 0`. Only `ep` may take no parameters.
- **Redefining a `fn` silently does nothing.** `NODE_FN` skips the duplicate check that
  `NODE_ASSIGN` has, and lookup returns the *first* match, so the old definition keeps winning with
  no error. Use `reset 0` to clear a session.
- **Lambdas must be parenthesized**: `(\x -> x + 1)`, and may be applied directly: `(\x -> x + 1) 5`.
- **Comparisons don't chain** — `a == b == c` is invalid.
- **Every `if`/`else` shares one `end`**, not one per branch. An `if` with no `else` is the normal
  way to write an early return.
- Calling with fewer args than declared **curries** rather than erroring.
- `ts` promises every recursive call is in tail position; the interpreter trampolines those in
  constant stack. Only tag a function `ts` when the recursive call is the literal `return`ed
  expression. Non-tail statements in a `ts` body (`print x`, `sleep n`) are fine.

## Entry point and looping

Exactly one `ep` function, last in the file, taking **no** parameters:

```
ep fn main -> :
    ...
end
```

Two looping idioms:
- `return main` from the entry point re-runs it forever (no state carried).
- A `ts` helper that tail-calls itself, carrying state in parameters. This is the only way to keep
  state across iterations, since there are no mutable variables.

Multiple values cannot be returned. Pack them into one int with shifts, e.g.
`return (seed << 6) | (y << 3) | x`, and unpack with `>>` and `&`.

## Size limits — check these before writing

- **`STMT_MAX` 4096 bytes**: the whole program, comments and all, must fit in one batch submission.
- **`LINE_MAX` 128 bytes** per line.
- Arenas (`repl/src/mu_arena.c`): session 65536, scratch 69632.

`examples/snake.mu` sits near the 4096 ceiling, so keep comments lean. Measure with `wc -c`.

## Memory and lifecycle

The interpreter allocates from a bump arena with no per-object free. It stays flat because:
- non-binding statements are rewound after they run,
- `x = expr` keeps the scalar and rewinds the working,
- the `ts` trampoline lifts the pending call's int arguments clear and rewinds each iteration.

A submission is kept only if it ran to completion **and** defined something. An error, a ctrl-c, or
`halt` rolls the whole submission back — arena and bindings — so nothing leaks. Ctrl-c is checked
in `evaluate_tc`, so it interrupts a running program. `reset 0` clears everything and re-registers
builtins.

## Worked examples

Currying and closures:

```
fn add -> a b:
    return a + b
end
add5 = add 5

fn makeAdder -> n:
    return (\x -> x + n)
end

ep fn main -> :
    add10 = makeAdder 10
    print (add5 10)     // 15
    return add10 32     // 42
end
```

Tail recursion carrying state:

```
ts fn fact -> acc n:
    if n == 0:
        return acc
    end
    return fact (acc * n) (n - 1)
end

ep fn main -> :
    print (fact 1 5)    // 120
    return 0
end
```

Animating in place with ANSI escapes (see `examples/game-of-life.mu`):

```
ts fn loop -> g:
    write "\e[H"                        // home the cursor, no newline
    print "\e[1;38;5;51m  Title\e[0m\e[K"
    write "\e[38;5;220m  gen "
    write g
    print "\e[0m\e[K"                   // \e[K clears any stale tail
    sleep 200
    return loop (g + 1)
end

ep fn main -> :
    print "\e[2J\e[?25l"                // clear once, hide the cursor
    return loop 1
end
```

The REPL re-emits `ESC[0m ESC[?25h` before every prompt, so a program that hides the cursor or sets
a colour never leaks it into the prompt.

## Checklist before handing over a program

1. Every `fn`/`ts fn` ends with `end`; every `if`/`else` shares one `end`.
2. Exactly one `ep fn <name> -> :`, no params, placed last.
3. Only the nine real builtins; no lists; no characters outside the recognised set.
4. Negative literal *arguments* and bare lambdas parenthesised; `~` leading or parenthesised.
5. No name assigned twice across nested scopes; helpers take at least one parameter.
6. `wc -c` under 4096, longest line under 128.

## Building and running

`parser.c`/`tokeniser.c` include `<zephyr/kernel.h>`, so a plain `gcc` syntax check will not work.
Build through the west workspace:

```bash
./repl/build.sh native_sim          # activates ~/zephyrproject, builds repl/build
```

For the nucleo, the installed Zephyr SDK version is rejected by this tree, so pass a system
cross-compiler:

```bash
export ZEPHYR_TOOLCHAIN_VARIANT=cross-compile CROSS_COMPILE=/usr/bin/arm-none-eabi-
west build -b nucleo_f429zi -s repl -d /tmp/build_nucleo
```

To actually run a program on `native_sim`, the binary needs a **tty** and a startup delay — piping
a file straight in silently does nothing. Feed it through a pty, in batch mode (ctrl-e … ctrl-d),
with the pty in raw mode so ctrl-d is not eaten:

```bash
( sleep 2; printf '\x05'; sleep 0.5; cat prog.mu; sleep 1; printf '\x04'; sleep 20 ) \
  | timeout 30 script -qec "stty raw -echo; ./repl/build/zephyr/zephyr.exe -uart_stdinout" /dev/null \
  | tr -d '\r'
```

Batch mode echoes the source before running it, so when counting output, restrict to the region
after the `=== output ===` separator or you will match your own program text.

Hardware programs cannot run on `native_sim` (`device_get_binding` finds nothing). Keep the
hardware read in a one-line function so it can be stubbed:
`fn rx -> u: return sgn (i2cRegRead "i2c1" 29 1) end` → swap the body for `return 0` to test the
logic on the simulator. Every example in `examples/` is written this way.

On real hardware use `./repl/upload.sh prog.mu /dev/ttyACM0`.
