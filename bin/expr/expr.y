/* $NetBSD: expr.y,v 1.39 2016/09/05 01:00:07 sevan Exp $ */

/*_
 * Copyright (c) 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jaromir Dolecek <jdolecek@NetBSD.org> and J.T. Conklin <jtc@NetBSD.org>.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

%{
#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: expr.y,v 1.39 2016/09/05 01:00:07 sevan Exp $");
#endif /* not lint */

#include <sys/types.h>

#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char * const *av;

static void yyerror(const char *, ...) __dead;
static int yylex(void);
static int is_zero_or_null(const char *);
static int is_integer(const char *);
static int64_t perform_arith_op(const char *, const char *, const char *);

#define YYSTYPE	const char *

%}
%token STRING
%left SPEC_OR
%left SPEC_AND
%left COMPARE 
%left ADD_SUB_OPERATOR
%left MUL_DIV_MOD_OPERATOR
%left SPEC_REG
%left LENGTH
%left LEFT_PARENT RIGHT_PARENT

%%

exp:	expr = {
		(void) printf("%s\n", $1);
		return (is_zero_or_null($1));
		}
	;

expr:	item { $$ = $1; }
	| expr SPEC_OR expr = {
		/*
		 * Return evaluation of first expression if it is neither
		 * an empty string nor zero; otherwise, returns the evaluation
		 * of second expression.
		 */
		if (!is_zero_or_null($1))
			$$ = $1;
		else
			$$ = $3;
		}
	| expr SPEC_AND expr = {
		/*
		 * Returns the evaluation of first expr if neither expression
		 * evaluates to an empty string or zero; otherwise, returns
		 * zero.
		 */
		if (!is_zero_or_null($1) && !is_zero_or_null($3))
			$$ = $1;
		else
			$$ = "0";
		}
	| expr SPEC_REG expr = {
		/*
		 * The ``:'' operator matches first expr against the second,
		 * which must be a regular expression.
		 */
		regex_t rp;
		regmatch_t rm[2];
		int eval;

		/* compile regular expression */
		if ((eval = regcomp(&rp, $3, REG_BASIC)) != 0) {
			char errbuf[256];
			(void)regerror(eval, &rp, errbuf, sizeof(errbuf));
			yyerror("%s", errbuf);
			/* NOT REACHED */
		}
		
		/* compare string against pattern --  remember that patterns 
		   are anchored to the beginning of the line */
		if (regexec(&rp, $1, 2, rm, 0) == 0 && rm[0].rm_so == 0) {
			char *val;
			if (rm[1].rm_so >= 0) {
				(void) asprintf(&val, "%.*s",
					(int) (rm[1].rm_eo - rm[1].rm_so),
					$1 + rm[1].rm_so);
			} else {
				(void) asprintf(&val, "%d",
					(int)(rm[0].rm_eo - rm[0].rm_so));
			}
			if (val == NULL)
				err(1, NULL);
			$$ = val;
		} else {
			if (rp.re_nsub == 0) {
				$$ = "0";
			} else {
				$$ = "";
			}
		}

		}
	| expr ADD_SUB_OPERATOR expr = {
		/* Returns the results of addition, subtraction */
		char *val;
		int64_t res;
		
		res = perform_arith_op($1, $2, $3);
		(void) asprintf(&val, "%lld", (long long int) res);
		if (val == NULL)
			err(1, NULL);
		$$ = val;
                }

	| expr MUL_DIV_MOD_OPERATOR expr = {
		/* 
		 * Returns the results of multiply, divide or remainder of 
		 * numeric-valued arguments.
		 */
		char *val;
		int64_t res;

		res = perform_arith_op($1, $2, $3);
		(void) asprintf(&val, "%lld", (long long int) res);
		if (val == NULL)
			err(1, NULL);
		$$ = val;

		}
	| expr COMPARE expr = {
		/*
		 * Returns the results of integer comparison if both arguments
		 * are integers; otherwise, returns the results of string
		 * comparison using the locale-specific collation sequence.
		 * The result of each comparison is 1 if the specified relation
		 * is true, or 0 if the relation is false.
		 */

		int64_t l, r;
		int res;

		res = 0;

		/*
		 * Slight hack to avoid differences in the compare code
		 * between string and numeric compare.
		 */
		if (is_integer($1) && is_integer($3)) {
			/* numeric comparison */
			l = strtoll($1, NULL, 10);
			r = strtoll($3, NULL, 10);
		} else {
			/* string comparison */
			l = strcoll($1, $3);
			r = 0;
		}

		switch($2[0]) {	
		case '=': /* equal */
			res = (l == r);
			break;
		case '>': /* greater or greater-equal */
			if ($2[1] == '=')
				res = (l >= r);
			else
				res = (l > r);
			break;
		case '<': /* lower or lower-equal */
			if ($2[1] == '=')
				res = (l <= r);
			else
				res = (l < r);
			break;
		case '!': /* not equal */
			/* the check if this is != was done in yylex() */
			res = (l != r);
		}

		$$ = (res) ? "1" : "0";

		}
	| LEFT_PARENT expr RIGHT_PARENT { $$ = $2; }
	| LENGTH expr {
		/*
		 * Return length of 'expr' in bytes.
		 */
		char *ln;

		asprintf(&ln, "%ld", (long) strlen($2));
		if (ln == NULL)
			err(1, NULL);
		$$ = ln;
		}
	;

item:	STRING
	| ADD_SUB_OPERATOR
	| MUL_DIV_MOD_OPERATOR
	| COMPARE
	| SPEC_OR
	| SPEC_AND
	| SPEC_REG
	| LENGTH
	;
%%

/*
 * Returns 1 if the string is empty or contains only numeric zero.
 */
static int
is_zero_or_null(const char *str)
{
	char *endptr;

	return str[0] == '\0'
		|| ( strtoll(str, &endptr, 10) == 0LL
			&& endptr[0] == '\0');
}

/*
 * Returns 1 if the string is an integer.
 */
static int
is_integer(const char *str)
{
	char *endptr;

	(void) strtoll(str, &endptr, 10);
	/* note we treat empty string as valid number */
	return (endptr[0] == '\0');
}

static int64_t
perform_arith_op(const char *left, const char *op, const char *right)
{
	int64_t res, sign, l, r;
	u_int64_t temp;

	res = 0;

	if (!is_integer(left)) {
		yyerror("non-integer argument '%s'", left);
		/* NOTREACHED */
	}
	if (!is_integer(right)) {
		yyerror("non-integer argument '%s'", right);
		/* NOTREACHED */
	}

	errno = 0;
	l = strtoll(left, NULL, 10);
	if (errno == ERANGE) {
		yyerror("value '%s' is %s is %lld", left,
		    (l > 0) ? "too big, maximum" : "too small, minimum",
		    (l > 0) ? LLONG_MAX : LLONG_MIN);
		/* NOTREACHED */
	}

	errno = 0;
	r = strtoll(right, NULL, 10);
	if (errno == ERANGE) {
		yyerror("value '%s' is %s is %lld", right,
		    (l > 0) ? "too big, maximum" : "too small, minimum",
	  	    (l > 0) ? LLONG_MAX : LLONG_MIN);
		/* NOTREACHED */
	}

	switch(op[0]) {
	case '+':
		/* 
		 * Do the op into an unsigned to avoid overflow and then cast
		 * back to check the resulting signage. 
		 */
		temp = l + r;
		res = (int64_t) temp;
		/* very simplistic check for over-& underflow */
		if ((res < 0 && l > 0 && r > 0)
	  	    || (res > 0 && l < 0 && r < 0)) 
			yyerror("integer overflow or underflow occurred for "
                            "operation '%s %s %s'", left, op, right);
		break;
	case '-':
		/* 
		 * Do the op into an unsigned to avoid overflow and then cast
		 * back to check the resulting signage. 
		 */
		temp = l - r;
		res = (int64_t) temp;
		/* very simplistic check for over-& underflow */
		if ((res < 0 && l > 0 && l > r)
		    || (res > 0 && l < 0 && l < r) ) 
			yyerror("integer overflow or underflow occurred for "
			    "operation '%s %s %s'", left, op, right);
		break;
	case '/':
		if (r == 0) 
			yyerror("second argument to '%s' must not be zero", op);
		res = l / r;
			
		break;
	case '%':
		if (r == 0)
			yyerror("second argument to '%s' must not be zero", op);
		res = l % r;
		break;
	case '*':
		/* shortcut */
		if ((l == 0) || (r == 0)) {
			res = 0;
			break;
		}
				
		sign = 1;
		if (l < 0)
			sign *= -1;
		if (r < 0)
			sign *= -1;

		res = l * r;
		/*
		 * XXX: not the most portable but works on anything with 2's
		 * complement arithmetic. If the signs don't match or the
		 * result was 0 on 2's complement this overflowed.
		 */
		if ((res < 0 && sign > 0) || (res > 0 && sign < 0) || 
		    (res == 0))
			yyerror("integer overflow or underflow occurred for "
			    "operation '%s %s %s'", left, op, right);
			/* NOTREACHED */
		break;
	}
	return res;
}

static const char *x = "|&=<>+-*/%:()";
static const int x_token[] = {
	SPEC_OR, SPEC_AND, COMPARE, COMPARE, COMPARE, ADD_SUB_OPERATOR,
	ADD_SUB_OPERATOR, MUL_DIV_MOD_OPERATOR, MUL_DIV_MOD_OPERATOR, 
	MUL_DIV_MOD_OPERATOR, SPEC_REG, LEFT_PARENT, RIGHT_PARENT
};

static int handle_ddash = 1;

int
yylex(void)
{
	const char *p = *av++;
	int retval;

	if (!p)
		retval = 0;
	else if (p[1] == '\0') {
		const char *w = strchr(x, p[0]);
		if (w) {
			retval = x_token[w-x];
		} else {
			retval = STRING;
		}
	} else if (p[1] == '=' && p[2] == '\0'
			&& (p[0] == '>' || p[0] == '<' || p[0] == '!'))
		retval = COMPARE;
	else if (handle_ddash && p[0] == '-' && p[1] == '-' && p[2] == '\0') {
		/* ignore "--" if passed as first argument and isn't followed
		 * by another STRING */
		retval = yylex();
		if (retval != STRING && retval != LEFT_PARENT
		    && retval != RIGHT_PARENT) {
			/* is not followed by string or parenthesis, use as
			 * STRING */
			retval = STRING;
			av--;	/* was increased in call to yylex() above */
			p = "--";
		} else {
			/* "--" is to be ignored */
			p = yylval;
		}
	} else if (strcmp(p, "length") == 0)
		retval = LENGTH;
	else
		retval = STRING;

	handle_ddash = 0;
	yylval = p;

	return retval;
}

/*
 * Print error message and exit with error 2 (syntax error).
 */
static __printflike(1, 2) void
yyerror(const char *fmt, ...)
{
	va_list arg;

	va_start(arg, fmt);
	verrx(2, fmt, arg);
	va_end(arg);
}

int
main(int argc, const char * const *argv)
{
	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");

	if (argc == 1) {
		(void)fprintf(stderr, "usage: %s expression\n",
		    getprogname());
		exit(2);
	}

	av = argv + 1;

	exit(yyparse());
	/* NOTREACHED */
}
