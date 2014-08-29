%{
/*	$NetBSD: arith.y,v 1.22 2012/03/20 18:42:29 matt Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)arith.y	8.3 (Berkeley) 5/4/95";
#else
__RCSID("$NetBSD: arith.y,v 1.22 2012/03/20 18:42:29 matt Exp $");
#endif
#endif /* not lint */

#include <stdlib.h>
#include "expand.h"
#include "builtins.h"
#include "shell.h"
#include "error.h"
#include "output.h"
#include "memalloc.h"

typedef intmax_t YYSTYPE;
#define YYSTYPE YYSTYPE

intmax_t arith_result;
const char *arith_buf, *arith_startbuf;

__dead static void yyerror(const char *);
#ifdef TESTARITH
int main(int , char *[]);
int error(char *);
#endif

%}
%token ARITH_NUM ARITH_LPAREN ARITH_RPAREN

%left ARITH_OR
%left ARITH_AND
%left ARITH_BOR
%left ARITH_BXOR
%left ARITH_BAND
%left ARITH_EQ ARITH_NE
%left ARITH_LT ARITH_GT ARITH_GE ARITH_LE
%left ARITH_LSHIFT ARITH_RSHIFT
%left ARITH_ADD ARITH_SUB
%left ARITH_MUL ARITH_DIV ARITH_REM
%left ARITH_UNARYMINUS ARITH_UNARYPLUS ARITH_NOT ARITH_BNOT
%%

exp:	expr {
			/*
			 * yyparse() returns int, so we have to save
			 * the desired result elsewhere.
			 */
			arith_result = $1;
		}
	;


expr:	ARITH_LPAREN expr ARITH_RPAREN { $$ = $2; }
	| expr ARITH_OR expr	{ $$ = $1 ? $1 : $3 ? $3 : 0; }
	| expr ARITH_AND expr	{ $$ = $1 ? ( $3 ? $3 : 0 ) : 0; }
	| expr ARITH_BOR expr	{ $$ = $1 | $3; }
	| expr ARITH_BXOR expr	{ $$ = $1 ^ $3; }
	| expr ARITH_BAND expr	{ $$ = $1 & $3; }
	| expr ARITH_EQ expr	{ $$ = $1 == $3; }
	| expr ARITH_GT expr	{ $$ = $1 > $3; }
	| expr ARITH_GE expr	{ $$ = $1 >= $3; }
	| expr ARITH_LT expr	{ $$ = $1 < $3; }
	| expr ARITH_LE expr	{ $$ = $1 <= $3; }
	| expr ARITH_NE expr	{ $$ = $1 != $3; }
	| expr ARITH_LSHIFT expr { $$ = $1 << $3; }
	| expr ARITH_RSHIFT expr { $$ = $1 >> $3; }
	| expr ARITH_ADD expr	{ $$ = $1 + $3; }
	| expr ARITH_SUB expr	{ $$ = $1 - $3; }
	| expr ARITH_MUL expr	{ $$ = $1 * $3; }
	| expr ARITH_DIV expr	{
			if ($3 == 0)
				yyerror("division by zero");
			$$ = $1 / $3;
			}
	| expr ARITH_REM expr   {
			if ($3 == 0)
				yyerror("division by zero");
			$$ = $1 % $3;
			}
	| ARITH_NOT expr	{ $$ = !($2); }
	| ARITH_BNOT expr	{ $$ = ~($2); }
	| ARITH_SUB expr %prec ARITH_UNARYMINUS { $$ = -($2); }
	| ARITH_ADD expr %prec ARITH_UNARYPLUS { $$ = $2; }
	| ARITH_NUM
	;
%%
intmax_t
arith(const char *s)
{
	intmax_t result;

	arith_buf = arith_startbuf = s;

	INTOFF;
	(void) yyparse();
	result = arith_result;
	arith_lex_reset();	/* reprime lex */
	INTON;

	return (result);
}


/*
 *  The exp(1) builtin.
 */
int
expcmd(int argc, char **argv)
{
	const char *p;
	char *concat;
	char **ap;
	intmax_t i;

	if (argc > 1) {
		p = argv[1];
		if (argc > 2) {
			/*
			 * concatenate arguments
			 */
			STARTSTACKSTR(concat);
			ap = argv + 2;
			for (;;) {
				while (*p)
					STPUTC(*p++, concat);
				if ((p = *ap++) == NULL)
					break;
				STPUTC(' ', concat);
			}
			STPUTC('\0', concat);
			p = grabstackstr(concat);
		}
	} else
		p = "";

	(void)arith(p);
	i = arith_result;

	out1fmt("%"PRIdMAX"\n", i);
	return (! i);
}

/*************************/
#ifdef TEST_ARITH
#include <stdio.h>
main(argc, argv)
	char *argv[];
{
	printf("%"PRIdMAX"\n", exp(argv[1]));
}
error(s)
	char *s;
{
	fprintf(stderr, "exp: %s\n", s);
	exit(1);
}
#endif

static void
yyerror(const char *s)
{

	yyerrok;
	yyclearin;
	arith_lex_reset();	/* reprime lex */
	error("arithmetic expression: %s: \"%s\"", s, arith_startbuf);
	/* NOTREACHED */
}
