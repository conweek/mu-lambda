(*
 * Mu-Lambda EBNF Grammar
 *
 * Kept in step with repl/src/parser.c, which is the ground truth. If the two
 * disagree, the parser wins and this file is the bug.
 *
 * Conventions:
 *   non_terminal   = lowercase, snake_case
 *   TERMINAL        = UPPERCASE
 *   |               = alternation
 *   { ... }         = zero or more repetitions
 *   [ ... ]         = optional (zero or one)
 *   ( ... )         = grouping
 *)

program             = { statement NEWLINE } [ EP ( tsStatement | fnStatement ) NEWLINE ] EOF
statement           = fnStatement | tsStatement | ifStatement | returnStatement
                    | assignmentStatement | call
ifStatement         = IF exprStatement COLON NEWLINE { statement NEWLINE } [ ELSE COLON NEWLINE { statement NEWLINE } ] END
tsStatement         = TS fnStatement
fnStatement         = FN IDENTIFIER ARROW { IDENTIFIER } COLON NEWLINE block
block               = statement NEWLINE { statement NEWLINE } END
returnStatement     = RETURN exprStatement
assignmentStatement = IDENTIFIER ASSIGNMENT exprStatement
exprStatement       = comparison
comparison          = ( lambda | term ) [ op ( lambda | term ) ]
op                  = EQUALTO | NOTEQUALTO | GREATERTHAN | GREATERTHANEQUAL
                    | LESSTHAN | LESSTHANEQUAL
lambda              = OPENPAREN LAMBDA IDENTIFIER { IDENTIFIER } ARROW exprStatement CLOSEPAREN [ atomic { atomic } ]
term                = [ MINUS ] factor { ( PLUS | MINUS ) factor }
factor              = bitwise { ( TIMES | DIVIDE | MODULO ) bitwise }
bitwise             = [ COMPLIMENT ] ( call | atomic ) { bitop ( call | atomic ) }
bitop               = AND | OR | XOR | LSHIFT | RSHIFT
call                = IDENTIFIER { atomic | lambda }
atomic              = INT | STR | IDENTIFIER | OPENPAREN exprStatement CLOSEPAREN

(*
 * Precedence, loosest to tightest:
 *   comparison   ==  !=  >  >=  <  <=     single level, does NOT chain
 *   term         +  -                     and unary -
 *   factor       *  /  %
 *   bitwise      &  |  ^  <<  >>          and unary ~
 *   call         application, tightest, and takes only atomic arguments
 *
 * Bitwise binds TIGHTER than arithmetic, which is the opposite of C:
 *   1 & 3 + 4    parses as  (1 & 3) + 4    = 5
 *   2 * 3 & 1    parses as  2 * (3 & 1)    = 2
 *   1 << 3 + 1   parses as  (1 << 3) + 1   = 9
 *
 * COMPLIMENT is only accepted before the FIRST operand of a bitwise chain,
 * so `a & ~b` is a parse error. Write `a & (~b)` or `~b & a`.
 *
 * Application only consumes atomics, so `f x + 1` is `(f x) + 1`, and a bare
 * negative literal is a subtraction: `f -5` is `f - 5`. Write `f (-5)`.
 * A call with no arguments is just a variable reference, so a function
 * declared with no parameters can never be invoked. Only EP may take none.
 *
 * assignmentStatement and call both begin with IDENTIFIER; the parser tells
 * them apart by looking ahead for ASSIGNMENT.
 *)

(* Built-ins, parsed as IDENTIFIERs, bound in repl/src/main.c *)
built-ins = print, write, halt, reset, sleep,
            gpioSet, gpioRead, i2cRegWrite, i2cRegRead
