(*
 * Mu-Lambda EBNF Grammar
 *
 * Conventions:
 *   non_terminal   = lowercase, snake_case
 *   TERMINAL        = UPPERCASE
 *   |               = alternation
 *   { ... }         = zero or more repetitions
 *   [ ... ]         = optional (zero or one)
 *   ( ... )         = grouping
 *)

program     = { block NEWLINE } EOF ;

block       = statement NEWLINE { statement NEWLINE } END ;

statement   = fnStatement
            | tsStatement
            | ifStatement
            | returnStatement
            | assignmentStatement
            | exprStatement

ifStatement = IF expr COLON NEWLINE { statement NEWLINE} (END | [ ELSE COLON NEWLINE {statement NEWLINE} END])

fnStatement = FN IDENTIFIER ARROW { IDENTIFIER } COLON NEWLINE block

tsStatement = TC FN IDENTIFIER ARROW { IDENTIFIER } block ;

lambda      = LAMBDA { IDENTIFIER } ARROW expr ;

expr        = lambda
            | dollar ;

dollar      = comparison [ DOLLARSIGN dollar ] ;

comparison  = addition [ ( EQUALTO | NOTEQUALTO | GREATERTHAN | LESSTHAN ) addition ] ;

addition    = application { ( PLUS | MINUS ) application } ;

application = atomic { atomic } ;

atomic      = INT
            | STR
            | LIST
            | IDENTIFIER
            | OPENPAREN expr CLOSEPAREN ;


(* --- Example program --- *)
(*
succ = (\x -> x + 1)
succ (1 + 10)

fn name -> arg1 arg2:
    aslkjgh
    akjsfhd
    aksjdfh
    return akjsfhd
end
*)
