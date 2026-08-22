---
name: mu-lambda
description: Use when the user asks to write, generate, review, or fix a program in μλ (mu-lambda) — the small functional language implemented by this repo's repl/ (tokeniser.c, parser.c, interpreter.c). Trigger on requests like "write a mu-lambda program that...", "give me a μλ function for...", "why won't this μλ program parse", or work touching docs/rules.md, docs/language-reference.md, or repl/program.txt-style source files.
---

# μλ (Mu-Lambda)

A small, purely functional, Haskell/Python-flavored language, tokenised/parsed/interpreted by
`repl/src/{tokeniser,parser,interpreter}.c`. Ground truth for the grammar lives in
[docs/rules.md](../../../docs/rules.md) (formal EBNF) and
[docs/language-reference.md](../../../docs/language-reference.md) (prose + examples) — re-read
those two files if this skill's description of behavior ever seems to disagree with them, since
they're the canonical source and this file is a derived summary. Everything below was
cross-checked directly against the tokeniser/parser/interpreter source, so it also documents a
few real quirks the docs gloss over.

## Core rules

- No semicolons or statement terminators other than `\n`.
- No `for`/`while` loops — this is a pure functional language. The only repetition mechanisms are
  recursion and an entry-point function returning itself (see Entry point).
- No pattern matching.
- Two data types only: **integers** and **strings**. List syntax `[...]` is tokenised but the
  parser does not accept it anywhere — **never emit `[...]` in a program**, it will fail to parse.
- Variables are **immutable**: `name = expr` fails at runtime if `name` is already bound
  **anywhere in an enclosing scope**, not just the current block. This is stricter than normal
  shadowing — you cannot reuse an outer/global name for a new local via `=`, even inside a nested
  `if`/function body. Function parameters are the one exception (they bind fresh per call, not
  via `=`).
- Comments: `// rest of line`.
- Blocks (function bodies, `if`/`else` bodies) are closed with `end`, not indentation.
  Indentation is cosmetic only.

## Keywords, operators, literals

| Keyword | Meaning |
|---|---|
| `fn` | function definition |
| `ts` | tail-call signal, prefixes `fn` |
| `ep` | marks the program's entry-point function |
| `return` | return a value from a function body |
| `if` / `else` | conditional |
| `end` | closes a function/if/else block |

| Symbol | Meaning |
|---|---|
| `+ - * /` | arithmetic (ints only) |
| `> <` | comparison |
| `== !=` | (in)equality |
| `=` | assignment (first binding only, see immutability above) |
| `->` | separates a function's name from its params, and a lambda's params from its body |
| `:` | opens a block (`if cond:`, `fn f -> args:`) |
| `\` | starts a lambda: `\x -> expr` |
| `( )` | grouping / lambda literal wrapper / call-argument grouping |
| `"..."` | string literal |
| `123` | integer literal (decimal) |
| `identifier` | `[A-Za-z_][A-Za-z0-9_]*`, also how you call built-ins and functions |

Built-ins: **only `print`** is registered (`print x` — takes exactly one int or string argument,
prints it, evaluates to a no-result value). Anything else (`int`, `getLine`, string/list ops) does
not exist yet — do not call it.

## Grammar (as actually implemented by repl/src/parser.c)

```
program      = { statement NEWLINE } [ EP ( tsFn | fn ) NEWLINE ] EOF
block        = { statement NEWLINE } END
statement    = fn | tsFn | ifStmt | RETURN exprStatement
             | IDENTIFIER ASSIGNMENT exprStatement          // x = expr
             | IDENTIFIER { atomic }                        // bare call as a statement
fn           = FN IDENTIFIER ARROW { IDENTIFIER } COLON NEWLINE block
tsFn         = TS fn
ifStmt       = IF exprStatement COLON NEWLINE block-body
               [ ELSE COLON NEWLINE block-body ] END        // shares END with the if
exprStatement= comparison
comparison   = (lambda | term) [ (== | != | > | <) (lambda | term) ]   // NOT chainable
lambda       = "(" "\" IDENTIFIER { IDENTIFIER } "->" exprStatement ")" { atomic }
term         = [ "-" ] factor { ("+" | "-") factor }
factor       = (call | atomic) { ("*" | "/") (call | atomic) }
call         = IDENTIFIER { atomic }                        // curried application
atomic       = INT | STR | IDENTIFIER | "(" exprStatement ")"
```

Precedence, high to low: `* /`  >  `+ -` (with unary `-`)  >  comparison (single level, no
chaining: `a == b == c` is **not** valid — restructure with parens/helper calls instead).
Function/lambda application binds *tighter* than `+ -`, because `call`/`lambda` only consume
`atomic` arguments: `f x + 1` parses as `(f x) + 1`, not `f (x + 1)`.

### Gotchas worth remembering when generating code

- **A bare `-N` cannot be a call argument.** `atomic` doesn't include unary minus, so `f -5`
  parses as `f - 5` (binary subtraction on `f`, which will then fail at eval time unless `f` is an
  int). Always wrap negative literal arguments: `f (-5)`.
- **Lambdas must be parenthesized**, even as a bare expression: `\x -> x + 1` alone is not valid at
  statement/expr position — write `(\x -> x + 1)`. A parenthesized lambda can be immediately
  applied: `(\x -> x + 1) 5`.
- **Comparisons don't chain** and can't be nested arithmetic without parens on both sides if mixed
  with lambdas — write `(a > b)` explicitly when composing.
- **Every `if`/`else` still needs one shared `end`** — not one `end` per branch.
- Reassigning a name that exists in *any* enclosing scope is a runtime error — pick fresh names
  for locals that shadow outer ones.
- Calling a function with fewer args than declared **curries**: it returns a partially-applied
  closure rather than erroring. Calling with the exact remaining args resumes evaluation.
- `ts`-tagged functions promise the interpreter every recursive call is in tail position — the
  interpreter turns those into thunks and trampolines them so recursion runs in constant C-stack
  space. Tagging a non-tail-recursive function `ts` is documented as "will likely result in program
  death" — only use `ts` when the recursive call is the literal `return`ed expression.

## Entry point

Exactly one `ep`-marked function, and it must be the very last thing in the program (after every
other top-level `fn`/statement), and it must take **no** parameters:

```
ep fn main -> :
    ...
end
```

The interpreter calls this function. If its body evaluates to *the function's own closure value*
(i.e. literally `return main`), the interpreter loops and calls it again instead of terminating —
this is the only looping construct in the language, used for REPL-style repeat-until-done
programs.

## Worked examples

Currying:

```
fn add -> a b:
    return a + b
end

add5 = add 5

ep fn main -> :
    return add5 10   // 15
end
```

Closures / lambdas:

```
fn makeAdder -> n:
    return (\x -> x + n)
end

ep fn main -> :
    add10 = makeAdder 10
    return add10 32   // 42
end
```

Tail-recursive factorial with `print`:

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

## Writing a new program — checklist

1. Every `fn`/`ts fn` block ends with `end`; every `if`/`else` shares one `end`.
2. Exactly one `ep fn <name> -> :` (or `ep ts fn`), no params, placed last.
3. No `[...]` list literals, no unregistered built-ins (only `print` exists).
4. Negative literal arguments and bare lambdas are parenthesized (see Gotchas).
5. No name is assigned twice across nested scopes (function params excepted).
6. Comparisons are not chained.

## Verifying a program you wrote

`repl/src/parser.c` and `tokeniser.c` include `<zephyr/kernel.h>` directly (for `printk`/`k_malloc`),
so **you cannot syntax-check a program with a plain `gcc -I repl/include` build** — it fails with
`fatal error: zephyr/kernel.h: No such file or directory`. `tests/run.sh` (which wraps
`tests/src/parser-test.c` + `tests/src/tokeniser-test.c`) has the same requirement and will fail
the same way unless Zephyr's headers are already on the include path.

The real build goes through the Zephyr west workspace: see [repl/build.sh](../../../repl/build.sh)
(defaults to `native_sim`), which activates the separate `~/zephyrproject` west workspace and runs
`west build -b native_sim -s repl -d repl/build`. Once that's built:

```bash
./repl/build/zephyr/zephyr.exe -uart_stdinout      # interactive REPL
./repl/upload.sh program.txt <device>              # batch-run a source file (or pipe to native_sim's stdin equivalent)
```

Offer to build/run this way if the user wants to see actual output. If only a syntax sanity-check
is needed and a Zephyr toolchain isn't available, review the program by hand against the grammar
and gotchas above rather than claiming it was executed.
