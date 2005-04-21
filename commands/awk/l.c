/*
 * a small awk clone
 *
 * (C) 1989 Saeko Hirabauashi & Kouichi Hirabayashi
 *
 * Absolutely no warranty. Use this software with your own risk.
 *
 * Permission to use, copy, modify and distribute this software for any
 * purpose and without fee is hereby granted, provided that the above
 * copyright and disclaimer notice.
 *
 * This program was written to fit into 64K+64K memory of the Minix 1.2.
 */


#include <stdio.h>
#include <ctype.h>
#include "awk.h"

extern char *srcprg;	/* inline program */
extern FILE *pfp;	/* program file */

int sym;	/* lexical token */
int sym1;	/* auxiliary lexical token */
int regexflg;	/* set by parser (y.c) to indicate parsing REGEXPR */
int funflg;	/* set by parser (y.c) to indicate parsing FUNCTION */
int printflg;	/* set by parser (y.c) to indicate parsing PRINT */
int getlineflg;	/* set by parser (y.c) to indicate parsing GETLINE */
char text[BUFSIZ];	/* lexical word */
char line[BUFSIZ];	/* program line for error message (ring buffer) */
char *linep = line;	/* line pointer */
char funnam[128];	/* function name for error message */
int lineno = 1;

lex()
{
  int c, d;
  char *s;

  if (regexflg)
	return sym = scanreg();
next:
  while ((c = Getc()) == ' ' || c == '\t')
	;
  while (c == '#')
	for (c = Getc(); c != '\n'; c = Getc())
		;
  switch (c) {
  case '\\':
	if ((c = Getc()) == '\n') {
		lineno++;
		goto next;
	}
	break;
  case '\n':
	lineno++;
	break;
  }
  switch (c) {
  case EOF:	return sym = 0;
  case '+':	return sym = follow2('=', '+', ADDEQ, INC, ADD);
  case '-':	return sym = follow2('=', '-', SUBEQ, DEC, SUB);
  case '*':	return sym = follow('=', MULTEQ, MULT);
  case '/':	return sym = follow('=', DIVEQ, DIV);
  case '%':	return sym = follow('=', MODEQ, MOD);
  case '^':	return sym = follow('=', POWEQ, POWER);
  case '=':	return sym = follow('=', EQ, ASSIGN);
  case '!':	return sym = follow2('=', '~', NE, NOMATCH, NOT);
  case '&':	return sym = follow('&', AND, BINAND);
  case '|':	sym = follow('|', OR, BINOR);
		if (printflg && sym == BINOR)
			sym = R_POUT;
		return sym;
  case '<':	sym = follow2('=', '<', LE, SHIFTL, LT);
		if (getlineflg && sym == LT)
			sym = R_IN;
		return sym;
  case '>':	sym = follow2('=', '>', GE, SHIFTR, GT);
		if (printflg) {
			switch (sym) {
			case GT: sym = R_OUT; break;
			case SHIFTR: sym = R_APD; break;
			}
		}
		return sym;
  case '~':	return sym = MATCH; break;
  case ';': case '\n':	return sym = EOL;
  }
  if (isalpha(c) || c == '_') {
	for (s = text; isalnum(c) || c == '_'; ) {
		*s++ = c; c = Getc();
	}
	Ungetc(c);
	*s = '\0';
	if ((d = iskeywd(text)) == 0 &&
		(d = isbuiltin(text, &sym1)) == 0) {
			if (c == '(')
				return sym = CALL;
			else if (funflg) {
				if ((sym1 = isarg(text)) != -1)
					return sym = ARG;
			}
	}
	return sym = d ? d : IDENT;
  }
  else if (c == '.' || (isdigit(c))) {
	Ungetc(c);
	return sym = scannum(text);	/* NUMBER */
  }
  else if (c == '"')
	return sym = scanstr(text);	/* STRING */
  return sym = c;
}

static
follow(c1, r1, r2)
{
  register int c;

  if ((c = Getc()) == c1)
	return r1;
  else {
	Ungetc(c);
	return r2;
  }
}

static
follow2(c1, c2, r1, r2, r3)
{
  register int c;

  if ((c = Getc()) == c1)
	return r1;
  else if (c == c2)
	return r2;
  else {
	Ungetc(c);
	return r3;
  }
}

static
iskeywd(s) char *s;
{
  static struct { char *kw; int token; } tab[] = {
	"BEGIN", BEGIN,
	"END", END,
	"break", BREAK,
	"continue", CONTIN,
	"delete", DELETE,
	"do", DO,
	"else", ELSE,
	"exit", EXIT,
	"for", FOR,
	"func", FUNC,
	"function", FUNC,
	"getline", GETLINE,
	"if", IF,
	"in", IN,
	"next", NEXT,
	"print", PRINT,
	"printf", PRINTF,
	"return", RETURN,
	"sprint", SPRINT,
	"sprintf", SPRINTF,
	"while", WHILE,
	"", 0, 0
  };
  register int i;

  for (i = 0; tab[i].token; i++)
	if (strcmp(tab[i].kw, s) == 0)
		break;
  return tab[i].token;
}

static
isbuiltin(s, p) char *s; int *p;
{
  static struct { char *kw; int type; int token; } tab[] = {
	"atan2", MATHFUN, ATAN2,
	"close", STRFUN, CLOSE,
	"cos", MATHFUN, COS,
	"exp", MATHFUN, EXP,
	"gsub", SUBST, RGSUB,
	"index", STRFUN, INDEX,
	"int", MATHFUN, INT,
	"length", STRFUN, LENGTH,
	"log", MATHFUN, LOG,
	"match", STRFUN, RMATCH,
	"sin", MATHFUN, SIN,
	"sqrt", MATHFUN, SQRT,
	"rand", MATHFUN, RAND,
	"srand", MATHFUN, SRAND,
	"split", STRFUN, SPLIT,
	"sub", SUBST, RSUB,
	"substr", STRFUN, SUBSTR,
	"system", STRFUN, SYSTEM,
	"", 0, 0
  };
  register int i;

  for (i = 0; tab[i].token; i++)
	if (strcmp(tab[i].kw, s) == 0)
		break;
  *p = tab[i].token;
  return tab[i].type;
}

static
scannum(s) char *s;
{
  register int c;
  char *strchr();

  if ((c = Getc()) && strchr("+-", c) != NULL) {
	*s++ = c; c = Getc();
  }
  while (isdigit(c)) {
	*s++ = c; c = Getc();
  }
  if (c == '.') {
	*s++ = c; c = Getc();
	while (isdigit(c)) {
		*s++ = c; c = Getc();
	}
  }
  if (c && strchr("eE", c) != NULL) {
	*s++ = c; c = Getc();
	if (c && strchr("+-", c) != NULL) {
		*s++ = c; c = Getc();
	}
	while (isdigit(c)) {
		*s++ = c; c = Getc();
	}
  }
  *s = '\0';
  Ungetc(c);
  return NUMBER;
}

static
scanstr(s) char *s;
{
  register int c, i, j;

  for (c = Getc(); c != EOF & c != '"'; ) {
	if (c == '\\') {
		switch (c = Getc()) {
		case 'b': c = '\b'; break;
		case 'f': c = '\f'; break;
		case 'n': c = '\n'; break;
		case 'r': c = '\r'; break;
		case 't': c = '\t'; break;
		default:
		if (isdigit(c)) {
			for (i = j = 0; i < 3 && isdigit(c); c = Getc(), i++)
				j = j * 8 + c - '0';
			Ungetc(c);
			c = j;
		}
		break;
		}
	}
	*s++ = c;
	if (isKanji(c))
		*s++ = Getc();
	c = Getc();
  }
  *s = '\0';
  return STRING;
}

static
scanreg()
{
  register int c;
  register char *s;

  for (s = text; (c = Getc()) != '/'; )
	if (c == '\n')
		error("newline in regular expression");
	else {
		if (isKanji(c) || c == '\\') {
			*s++ = c; c = Getc();
		}
		*s++ = c;
	}
  *s = '\0';
  return REGEXP;
}

static int c0;

Ungetc(c)
{
  c0 = c;

  if (linep > line) {
	if (--linep < line)
		linep == line + BUFSIZ - 1;
  }
}

Getc()
{
  register int c;
  char *s, *t;

  if (c0) {
	c = c0; c0 = 0;
  }	
  else if (srcprg)
	c = *srcprg ? *srcprg++ : EOF;
  else
	c = fgetc(pfp);

#if 0
  if (linep - line == BUFSIZ) {
printf("!!!\n");
	for (s = line; *s != '\n' && ((s - line) <BUFSIZ); s++)
		;
printf("***(%d)***\n", *s);
	for (t = line; s < linep; )
		*t++ = *++s;
  }
#endif
  *linep++ = c;
  if ((linep - line) == BUFSIZ)
	linep = line;
  return c;
}
