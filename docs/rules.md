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

program             = { statement NEWLINE } [ EP ( tsStatement | fnStatement ) NEWLINE ] EOF
statement           = fnStatement | tsStatement | ifStatement | returnStatement | assignmentStatement
ifStatement         = IF exprStatement COLON NEWLINE { statement NEWLINE} [ ELSE COLON NEWLINE {statement NEWLINE} ] END
tsStatement         = TS fnStatement
fnStatement         = FN IDENTIFIER ARROW { IDENTIFIER } COLON NEWLINE block
block               = statement NEWLINE { statement NEWLINE } END
returnStatement     = RETURN exprStatement
assignmentStatement = IDENTIFIER ASSIGNMENT exprStatement
exprStatement       =  comparison | lambda | term
comparison          = (lambda | term) op (lambda | term)
op                  = EQUALTO | NOTEQUALTO | GREATERTHAN | LESSTHAN
lambda              = OPENPAREN LAMBDA IDENTIFIER { IDENTIFIER } ARROW exprStatement CLOSEPAREN [ atomic { atomic } ]
term                = [ MINUS ] ( atomic | call ) { ( PLUS | MINUS ) ( atomic | call ) }
call                = IDENTIFIER { atomic }
atomic              = INT | STR | IDENTIFIER | OPENPAREN exprStatement CLOSEPAREN
// Simple functions built into the language (parsed as IDENTIFIERS)
built-ins = print