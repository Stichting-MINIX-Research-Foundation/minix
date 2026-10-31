%{
/******************************************************************************
 *
 * Module Name: dtparser.y - Bison input file for table compiler parser
 *
 *****************************************************************************/

/*
 * Copyright (C) 2000 - 2019, Intel Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 */

#include "aslcompiler.h"

#define _COMPONENT          DT_COMPILER
        ACPI_MODULE_NAME    ("dtparser")

void *                      AslLocalAllocate (unsigned int Size);

/* Bison/yacc configuration */

#undef alloca
#define alloca              AslLocalAllocate

int                         DtParserlex (void);
int                         DtParserparse (void);
void                        DtParsererror (char const *msg);
extern char                 *DtParsertext;
extern DT_FIELD             *AslGbl_CurrentField;

UINT64                      DtParserResult; /* Expression return value */

/* Bison/yacc configuration */

#ifndef yytname
#define yytname             DtParsername
#endif
#define YYDEBUG             1               /* Enable debug output */
#define YYERROR_VERBOSE     1               /* Verbose error messages */
#define YYFLAG              -32768

/* Define YYMALLOC/YYFREE to prevent redefinition errors  */

#define YYMALLOC            malloc
#define YYFREE              free
%}

%union
{
     UINT64                 value;
     UINT32                 op;
}

/*! [Begin] no source code translation */

%type  <value>  Expression

%token <op>     OP_EXP_EOF
%token <op>     OP_EXP_NEW_LINE
%token <op>     OP_EXP_NUMBER
%token <op>     OP_EXP_HEX_NUMBER
%token <op>     OP_EXP_DECIMAL_NUMBER
%token <op>     OP_EXP_LABEL
%token <op>     OP_EXP_PAREN_OPEN
%token <op>     OP_EXP_PAREN_CLOSE

%left <op>      OP_EXP_LOGICAL_OR
%left <op>      OP_EXP_LOGICAL_AND
%left <op>      OP_EXP_OR
%left <op>      OP_EXP_XOR
%left <op>      OP_EXP_AND
%left <op>      OP_EXP_EQUAL OP_EXP_NOT_EQUAL
%left <op>      OP_EXP_GREATER OP_EXP_LESS OP_EXP_GREATER_EQUAL OP_EXP_LESS_EQUAL
%left <op>      OP_EXP_SHIFT_RIGHT OP_EXP_SHIFT_LEFT
%left <op>      OP_EXP_ADD OP_EXP_SUBTRACT
%left <op>      OP_EXP_MULTIPLY OP_EXP_DIVIDE OP_EXP_MODULO
%right <op>     OP_EXP_ONES_COMPLIMENT OP_EXP_LOGICAL_NOT

%%

/*
 *  Operator precedence rules (from K&R)
 *
 *  1)      ( )
 *  2)      ! ~ (unary operators that are supported here)
 *  3)      *   /   %
 *  4)      +   -
 *  5)      >>  <<
 *  6)      <   >   <=  >=
 *  7)      ==  !=
 *  8)      &
 *  9)      ^
 *  10)     |
 *  11)     &&
 *  12)     ||
 */
Value
    : Expression OP_EXP_NEW_LINE                     { DtParserResult=$1; return 0; } /* End of line (newline) */
    | Expression OP_EXP_EOF                          { DtParserResult=$1; return 0; } /* End of string (0) */
    ;

Expression

      /* Unary operators */

    : OP_EXP_LOGICAL_NOT         Expression          { $$ = DtDoOperator ($2, OP_EXP_LOGICAL_NOT,     $2);}
    | OP_EXP_ONES_COMPLIMENT     Expression          { $$ = DtDoOperator ($2, OP_EXP_ONES_COMPLIMENT, $2);}

      /* Binary operators */

    | Expression OP_EXP_MULTIPLY         Expression  { $$ = DtDoOperator ($1, OP_EXP_MULTIPLY,        $3);}
    | Expression OP_EXP_DIVIDE           Expression  { $$ = DtDoOperator ($1, OP_EXP_DIVIDE,          $3);}
    | Expression OP_EXP_MODULO           Expression  { $$ = DtDoOperator ($1, OP_EXP_MODULO,          $3);}
    | Expression OP_EXP_ADD              Expression  { $$ = DtDoOperator ($1, OP_EXP_ADD,             $3);}
    | Expression OP_EXP_SUBTRACT         Expression  { $$ = DtDoOperator ($1, OP_EXP_SUBTRACT,        $3);}
    | Expression OP_EXP_SHIFT_RIGHT      Expression  { $$ = DtDoOperator ($1, OP_EXP_SHIFT_RIGHT,     $3);}
    | Expression OP_EXP_SHIFT_LEFT       Expression  { $$ = DtDoOperator ($1, OP_EXP_SHIFT_LEFT,      $3);}
    | Expression OP_EXP_GREATER          Expression  { $$ = DtDoOperator ($1, OP_EXP_GREATER,         $3);}
    | Expression OP_EXP_LESS             Expression  { $$ = DtDoOperator ($1, OP_EXP_LESS,            $3);}
    | Expression OP_EXP_GREATER_EQUAL    Expression  { $$ = DtDoOperator ($1, OP_EXP_GREATER_EQUAL,   $3);}
    | Expression OP_EXP_LESS_EQUAL       Expression  { $$ = DtDoOperator ($1, OP_EXP_LESS_EQUAL,      $3);}
    | Expression OP_EXP_EQUAL            Expression  { $$ = DtDoOperator ($1, OP_EXP_EQUAL,           $3);}
    | Expression OP_EXP_NOT_EQUAL        Expression  { $$ = DtDoOperator ($1, OP_EXP_NOT_EQUAL,       $3);}
    | Expression OP_EXP_AND              Expression  { $$ = DtDoOperator ($1, OP_EXP_AND,             $3);}
    | Expression OP_EXP_XOR              Expression  { $$ = DtDoOperator ($1, OP_EXP_XOR,             $3);}
    | Expression OP_EXP_OR               Expression  { $$ = DtDoOperator ($1, OP_EXP_OR,              $3);}
    | Expression OP_EXP_LOGICAL_AND      Expression  { $$ = DtDoOperator ($1, OP_EXP_LOGICAL_AND,     $3);}
    | Expression OP_EXP_LOGICAL_OR       Expression  { $$ = DtDoOperator ($1, OP_EXP_LOGICAL_OR,      $3);}

      /* Parentheses: '(' Expression ')' */

    | OP_EXP_PAREN_OPEN          Expression
        OP_EXP_PAREN_CLOSE                           { $$ = $2;}

      /* Label references (prefixed with $) */

    | OP_EXP_LABEL                                   { $$ = DtResolveLabel (DtParsertext);}

      /*
       * All constants for the data table compiler are in hex, whether a (optional) 0x
       * prefix is present or not. For example, these two input strings are equivalent:
       *    1234
       *    0x1234
       */

      /* Non-prefixed hex number */

    | OP_EXP_NUMBER                                  { $$ = DtDoConstant (DtParsertext);}

      /* Standard hex number (0x1234) */

    | OP_EXP_HEX_NUMBER                              { $$ = DtDoConstant (DtParsertext);}

      /* Possible TBD: Decimal number with prefix (0d1234) - Not supported this time */

    | OP_EXP_DECIMAL_NUMBER                          { $$ = DtDoConstant (DtParsertext);}
    ;
%%

/*! [End] no source code translation !*/

/*
 * Local support functions, including parser entry point
 */
#define PR_FIRST_PARSE_OPCODE   OP_EXP_EOF
#define PR_YYTNAME_START        3


/******************************************************************************
 *
 * FUNCTION:    DtParsererror
 *
 * PARAMETERS:  Message             - Parser-generated error message
 *
 * RETURN:      None
 *
 * DESCRIPTION: Handler for parser errors
 *
 *****************************************************************************/

void
DtParsererror (
    char const              *Message)
{
    DtError (ASL_ERROR, ASL_MSG_SYNTAX,
        AslGbl_CurrentField, (char *) Message);
}


/******************************************************************************
 *
 * FUNCTION:    DtGetOpName
 *
 * PARAMETERS:  ParseOpcode         - Parser token (OP_EXP_*)
 *
 * RETURN:      Pointer to the opcode name
 *
 * DESCRIPTION: Get the ascii name of the parse opcode for debug output
 *
 *****************************************************************************/

char *
DtGetOpName (
    UINT32                  ParseOpcode)
{
#ifdef ASL_YYTNAME_START
    /*
     * First entries (PR_YYTNAME_START) in yytname are special reserved names.
     * Ignore first 6 characters of name (OP_EXP_)
     */
    return ((char *) yytname
        [(ParseOpcode - PR_FIRST_PARSE_OPCODE) + PR_YYTNAME_START] + 6);
#else
    return ("[Unknown parser generator]");
#endif
}


/******************************************************************************
 *
 * FUNCTION:    DtEvaluateExpression
 *
 * PARAMETERS:  ExprString          - Expression to be evaluated. Must be
 *                                    terminated by either a newline or a NUL
 *                                    string terminator
 *
 * RETURN:      64-bit value for the expression
 *
 * DESCRIPTION: Main entry point for the DT expression parser
 *
 *****************************************************************************/

UINT64
DtEvaluateExpression (
    char                    *ExprString)
{

    DbgPrint (ASL_DEBUG_OUTPUT,
        "**** Input expression: %s  (Base 16)\n", ExprString);

    /* Point lexer to the input string */

    if (DtInitLexer (ExprString))
    {
        DtError (ASL_ERROR, ASL_MSG_COMPILER_INTERNAL,
            AslGbl_CurrentField, "Could not initialize lexer");
        return (0);
    }

    /* Parse/Evaluate the input string (value returned in DtParserResult) */

    DtParserparse ();
    DtTerminateLexer ();

    DbgPrint (ASL_DEBUG_OUTPUT,
        "**** Parser returned value: %u (%8.8X%8.8X)\n",
        (UINT32) DtParserResult, ACPI_FORMAT_UINT64 (DtParserResult));

    return (DtParserResult);
}
