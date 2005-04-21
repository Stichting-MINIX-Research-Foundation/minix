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
#include "awk.h"

extern char *mkpat();

extern char *cmd;
extern char text[];
extern char funnam[];
extern int sym;
extern int sym1;
extern int regexflg;
extern int funflg;
extern int printflg;
extern int getlineflg;

extern SYMBOL *hashtab[], *funtab[];

extern CELL *field[];

char *emalloc(), *strsave();
NODE *node0(), *node1(), *node2(), *node3(), *node4();
NODE *stat(), *pastat();
NODE *expr(), *expr1(), *expr2(), *expr3(), *expr4();
NODE *expr5(), *expr6(), *expr7(), *expr8(), *expr9(), *expr10();
NODE *doprint(), *dofuncn(), *doif(), *dowhile(), *dofor(), *body();
NODE *doassign(), *dodo(), *doarray(), *doreturn(), *doelement();
CELL *mkcell(), *getvar();
CELL *execute(), *lookup();

int forflg;	/* parsing for(expr in array), inhibit 'expr in array' */
int prmflg;	/* parsing pass parameters */
NODE *begin, *loop, *End;

parse()
{
  NODE *p, *q, *r, *stat();
  CELL *u;

  lex();
  skipeol();
  while (sym) {
	switch (sym) {
	case BEGIN:
		lex();
		begin = stat();
		break;
	case END:
		lex();
		if (End == NULL)
			End = stat();
		else {
			for (p = End; p; p = q) {
				if ((q = p->n_next) == NULL)
					p->n_next = stat();
			}
		}
		break;
	case FUNC:
		lex();
		dousrfun();
		break;
	default:
		q = loop = pastat();
		skipeol();
		while (sym &&  sym != BEGIN && sym != END && sym != FUNC) {
			r = pastat();
			q->n_next = r;
			q = r;
			skipeol();
		}
		break;
	}
	skipeol();
  }
  if (begin) {
	u = execute(begin);
	c_free(u);
  }
  if (End || loop)
	while (Getrec(NULL)) {
		if (loop) {
			u = execute(loop);
			c_free(u);
		}
	}
  if (End) {
	u = execute(End);
	c_free(u);
  }
}

#define MAXARG		100
static char *argnam[MAXARG];
static int narg;

static
dousrfun()
{
  CELL *u;

  strcpy(funnam, text);
  u = getvar(text, funtab, FUN);
  lex();
  if (sym != '(')
	synerr("'(' expected");
  for (lex(); sym != ')'; narg++) {
	if (sym != IDENT)
		synerr("argument expected");
	argnam[narg] = strsave(text);
	lex();
	if (sym == ',')
		lex();
  }
  u->c_fval = (double) narg;
  lex();
  skipeol();
  funflg++;
  u->c_sval = (char *) stat();
  funflg--;
  if (narg > 0) {
	do {
		sfree(argnam[--narg]);
	} while (narg > 0);
  }
  skipeol();
}

isarg(s) char *s;
{
  int i;

  if (narg > 0) {
	for (i = narg - 1; i >= 0; i--)
		if (strcmp(s, argnam[i]) == 0)
			break;
  }
  else
	i = -1;
  return i;
}

/*
interactive()
{
  NODE *p, *q;
  CELL *u;

  for (lex(); sym; lex()) {
	p = stat();
	if (p->n_type != PRINT && !iscntl(p->n_type)) {
		q = (NODE *) emalloc(sizeof(NODE) + sizeof(NODE *) * 4);
		q->n_type = PRINT;
		q->n_arg[0] = q->n_arg[1] = q->n_arg[3] = NULL;
		q->n_arg[2] = p;
		q->n_next = NULL;
		p = q;
	}
	u = execute(p);
	printf("[%g(%s)]\n", u->c_fval, u->c_sval);
	c_free(u);
  }
  closeall();
  exit(0);
}
*/

static
iscntl(t)
{
  static int tab[] = {
	IF, DO, WHILE, FOR, JUMP, GETLINE, 0
  };
  int i;

  for (i = 0; tab[i]; i++)
	if (t == tab[i])
		break;
  return tab[i];
}

static NODE *
pastat()
{
  NODE *p, *q, *r;

  if (sym == '{')	/* action only */
	p = stat();
  else {	/* exp [, expr] [{ .. }] */
	p = expr();
	if (sym == ',') {
		lex();
		q = expr();
	}
	else
		q = NULL;
	if (sym && sym != EOL)
		r = stat();
	else
		r = node0(PRINT0);
	if (q)
		p = node3(P2STAT, p, q, r);
	else
		p = node2(P1STAT, p, r);
  }
  return p;
}

static NODE *
stat()
{
  NODE *p, *q, *r;
  CELL *u, *v;
  int op;

/*printf("@stat(%d)(%s)\n", sym, text);*/
  while (sym == EOL)
	lex();
  switch(sym) {
  case PRINT:
	p = doprint(0);
	break;
  case PRINTF:
	p = doprint(FORMAT);
	break;
  case IF:
	p = doif();
	break;
  case WHILE:
	p = dowhile();
	break;
  case DO:
	p = dodo();
	break;
  case FOR:
	p = dofor();
	break;
  case RETURN:
	p = doreturn();
	break;
  case EXIT:
	p = node2(JUMP, (NODE *)sym, (NODE *)NULL);
	lex();
	if (sym == IDENT || sym == NUMBER || sym == ARG)
		p->n_arg[1] = expr();
	break;
  case BREAK: case CONTIN: case NEXT:
	p = node1(JUMP, (NODE *)sym);
	lex();
	break;
  case DELETE:
	lex();
	u = getvar(text, hashtab, ARR);
	if (Getc() != '[')
		synerr("'[' expected");
	p = doarray(u);
	p->n_type = DELETE;
	lex();	/* ']' */
	break;
  case '{':
	lex();
	skipeol();
	if (sym == '}')
		p = node0(NULPROC);
	else
		p = q = stat();
	skipeol();
	while (sym != '}') {
		r = stat();
		q->n_next = r;
		q = r;
		skipeol();
	}
	lex();
	break;
  default:
	p = expr();
#if 0
	if (sym == BINOR) {	/* expr | GETLINE */
		lex();
		if (sym != GETLINE)
			synerr("'GETLINE' expected");
		lex();
		if (sym == IDENT || sym == STRING || sym == ARG) {
			q = expr();
		}
		else
			q = NULL;
		p = node3(GETLINE, q, p, (NODE *)R_PIN);
	}
#endif
	break;
  }
  if (p->n_type == VALUE)
	synerr("statement expected");
  return p;
}

static
skipeol()
{
  while (sym == EOL)
	lex();
}

static NODE *
doprint(fmt)
{
  NODE *p, *q, *r;
  CELL *u;
  int i, op;
  int n = 0;

  printflg++;
  lex();
  if (sym == '(')
	lex();
  if (sym != '}' && sym != ')' && sym != EOL && sym != R_OUT && sym != R_APD
	&& sym != R_POUT) {
	p = q = expr(); n++;
	while (sym == ',') {
  		lex();
		skipeol();
	  	r = expr(); n++;
		q->n_next = r;
	  	q = r;
	}
  }
  if (sym == ')')
	lex();
  if (sym == R_OUT || sym == R_APD || sym == R_POUT) {
	op = sym;
	lex();
/*	q = expr10();*/
	q = expr();	/* 94-04-02 */
  }
  else
	q = (NODE *) (op = 0);	/* stdout */
  printflg = 0;
  r = (NODE *) emalloc(sizeof(*r) + sizeof(r) * (n + 3));
  r->n_type = PRINT;	/* convert list to arg */
  r->n_next = NULL;
  r->n_arg[0] = (NODE *) (op | fmt);
  r->n_arg[1] = q;
  if (n == 0) {
	p = node1(VALUE, (NODE *)field[0]);
  }
  for (i = 2; p != NULL; i++) {
	r->n_arg[i] = p;
	q = p->n_next;
	p->n_next = NULL;
	p = q;
  }
  r->n_arg[i] = NULL;
  return r;
}

static NODE *
doif()
{
  NODE *p, *q, *r;

  lex();
  if (sym != '(')
	synerr("'(' expected");
  lex();
  p = expr();
  if (sym != ')')
	synerr("')' expected");
  lex();
  skipeol();
  q = stat();
  skipeol();
  if (sym == ELSE) {
	lex();
	skipeol();
	r = stat();
  }
  else
	r = NULL;
  return node3(IF, p, q, r);
}

static NODE *
dowhile()
{
  NODE *p, *q;

  lex();
  if (sym != '(')
	synerr("'(' expected");
  lex();
  p = stat();
  if (sym != ')')
	synerr("')' expected");
  q = body();
  return node2(WHILE, p, q);
}

static NODE *
dofor()
{
  NODE *p, *q, *r, *s;
  CELL *u;
  int i;

  lex();
  if (sym != '(')
	synerr("'(' expected");
  lex();
  if (sym != EOL) {
	forflg++;	/* inhibit parsing 'expr IN array' */
	p = expr();
	forflg = 0;
  }
  else
	p = NULL;
  if (sym == IN) {
	lex();
	if (sym == ARG) {
/*
printf("***FOR_IN_ARG(%d)***\n", sym);
*/
		u = mkcell(POS, NULL, (double)sym1);
		q = node1(ARG, u);
	}
	else {
		u = getvar(text, hashtab, ARR);
		q = node1(VALUE, u);
	}
	lex();
	if (sym != ')')
		synerr("')' expected");
	lex();
	skipeol();
	s = stat();
	r = node3(FORIN, p, q, s);
  }
  else {
	if (sym != EOL)
		synerr("'in' or ';' expected");
	lex();
	if (sym != EOL)
		q = expr();
	else
		q = NULL;
	if (sym != EOL)
		synerr("';' expected");
	lex();
	if (sym != ')')
		r = expr();
	else
		r = NULL;
	if (sym != ')')
		synerr("')' expected");
	s = body();
	r = node4(FOR, p, q, r, s);
  }
  return r;
}

static NODE *
body()
{
  NODE *r;

  while ((sym = Getc()) == '\n' || sym == ' ' || sym == '\t')
	;
  if (sym == ';') {
	r = node0(NULPROC);
	lex();
  }
  else {
	Ungetc(sym);
	lex();
	r = stat();
  }
  return r;
}

static NODE *
dodo()
{
  NODE *p, *q;

  lex();
  skipeol();
  p = stat();
  skipeol();
  if (sym != WHILE)
	synerr("'while' expected");
  lex();
  if (sym != '(')
	synerr("'(' expected");
  lex();
  q = stat();
  if (sym != ')')
	synerr("')' expected");
  lex();
  return node2(DO, p, q);
}

static NODE *
doreturn()
{
  NODE *p, *q, *r;
  int i, n = 0;

  if (lex() != EOL) {
	p = q = expr(); n++;
	while (sym == ',') {
		lex(); skipeol();
		r = expr(); n++;
		q ->n_next = r;
		q = r;
	}
  }
  else
	p = (NODE *)NULL;

  r = (NODE *) emalloc(sizeof(*r) + sizeof (r) * (n + 1));
  r->n_type = JUMP;
  r->n_next = NULL;
  r->n_arg[0] = (NODE *) RETURN;
  for (i = 1; p != NULL; i++) {
	r->n_arg[i] = p;
	q = p->n_next;
	p->n_next = NULL;
	p = q;
  }
  r->n_arg[i] = NULL;
  return r;
}

static NODE *
expr()
{
  NODE *p;

  p = expr1();
  if (isassign(sym))
	p = doassign(sym, p);
  return p;
}

static isassign(sym)
{
  return (sym == ASSIGN || sym == ADDEQ || sym == SUBEQ || sym == MULTEQ
  	|| sym == DIVEQ || sym == MODEQ || sym == POWEQ);
}

static NODE *
doassign(op, p) NODE *p;
{	/* evaluate right to left */
  NODE *q;

  lex();
  q = expr();
  if (isassign(sym))
	q = doassign(sym, q);
  return node3(ASSIGN, (NODE *)op, p, q);
}

static NODE *
expr1()
{
  NODE *p, *q;

/*
printf("expr1(%d)(%s)\n", sym, text);
*/
  p = expr2();
  if (sym == '?') {
	lex();
#if 0
	q = stat();
	if (sym != ':')
		synerr("':' expected");
	lex();
	return node3(IF, p, q, stat());
#else
	q = expr();
	if (sym != ':')
		synerr("':' expected");
	lex();
	return node3(IF, p, q, expr());
#endif
  }
  return p;	/* 930213 */
}

static NODE *
expr2()
{
  NODE *p;

/*
printf("expr2(%d)(%s)\n", sym, text);
*/
  p = expr3();
  while (sym == OR) {
	lex();
	skipeol();
	p = node3(COND, (NODE *)OR, p, expr3());
  }
  return p;
}

static NODE *
expr3()
{
  NODE *p;

/*
printf("expr3(%d)(%s)\n", sym, text);
*/
  p = expr4();
  while (sym == AND) {
	lex();
	skipeol();
	p = node3(COND, (NODE *)AND, p, expr4());
  }
  return p;
}

static NODE *
expr4()
{
  NODE *p;
  CELL *q;
  int op;

/*
printf("expr4(%d)(%s)\n", sym, text);
*/
  p = expr5();
  if (!forflg && sym == IN) {
	lex();
	q = getvar(text, hashtab, ARR);
	lex();
	return node2(IN, p, q);
  }
  while (sym == EQ || sym == NE || sym == LT || sym == LE  || sym == GT
	|| sym == GE || sym == MATCH || sym == NOMATCH) {
	op = sym;
	lex();
	p = node3(COND, (NODE *)op, p, expr5());
  }
  return p;
}

static NODE *
expr5()
{
  NODE *p, *q;

/*
printf("expr5(%d)(%s)\n", sym, text);
*/
  p = expr6();
  while (iscat(sym)) {
	q = expr6();
	p = node2(CAT, p, q);
  }
  return p;
}

static iscat(sym)
{
  static int ctab[] = {
	ADD, SUB, MULT, DIV, MOD, INC, DEC, STRING, NUMBER, IDENT, '(',
	MATHFUN, STRFUN, SPRINTF, '$', SUBST, ARG, CALL, 0
	};
  register int i, j;

  for (i = 0; j = ctab[i]; i++)
	if (sym == j)
		break;
  return j;
}

static NODE *
expr6()
{
  register int sign = sym;
  NODE *p, *q;

/*
printf("expr6(%d)(%s)\n", sym, text);
*/
  if (sym == SUB || sym == ADD)
	lex();
  p = expr7();
  if (sign == SUB)
	p = node2(ARITH, (NODE *)UMINUS, p);
  while (sym == ADD || sym == SUB) {
	sign = sym;
	lex();
	q = expr7();
	if (sign == ADD) {
		p = node3(ARITH, (NODE *)ADD, p, q);
	}
	else if (sign == SUB) {
		p = node3(ARITH, (NODE *)SUB, p, q);
	}
	else
		synerr("'+' or '-' expected");
  }
  return p;
}

static NODE *
expr7()
{
  register int op;
  NODE *p, *q;

/*
printf("expr7(%d)(%s)\n", sym, text);
*/
  p = expr8();
  while (sym == MULT || sym == DIV || sym == MOD) {
	op = sym;
	lex();
	q = expr8();
	switch (op) {
	case MULT:	p = node3(ARITH, (NODE *)MULT, p, q); break;
	case DIV:	p = node3(ARITH, (NODE *)DIV, p, q); break;
	case MOD:	p = node3(ARITH, (NODE *)MOD, p, q); break;
	default:	synerr("'*', '/' or '%' expected"); break;
	}
  }
  return p;
}

static NODE *
expr8()
{
  NODE *p;
  int op;

/*
printf("expr8(%d)(%s)\n", sym, text);
*/
  if (sym == NOT) {
	lex();
	p = node2(COND, (NODE *)NOT, expr9());
  }
  else {
	p = expr9();
	if (sym == POWER) {
		lex();
		p = node3(ARITH, (NODE *)POWER, p, expr9());
	}
  }
  return p;
}

static NODE *
expr9()
{
  NODE *p, *q;
  int op, sym0;

/*
printf("expr9(%d)(%s)\n", sym, text);
*/
  if (op = isincdec(sym)) {
	lex();
	if (sym != IDENT && sym != ARG)
		synerr("illegal '++/--' operator");
	p = expr10();
	p = node4(ARITH, (NODE *)INCDEC, p, (NODE *)op, (NODE *)PRE);
  }
  else {
	sym0 = sym;
	p = expr10();
	if (op = isincdec(sym)) {
/*printf("POST(%d)(%d)(%s)\n", sym, sym0, text);*/
		if (sym0 == IDENT || sym0 == ARG) {
			p = node4(ARITH, (NODE *)INCDEC, p, (NODE *)op,
				(NODE *)POST);
			lex();
		}
	}
	if (sym == BINOR) {	/* | getline */
		lex();
		if (sym != GETLINE)
			synerr("'GETLINE' expected");
		lex();
		if (sym == IDENT || sym == STRING || sym == ARG) {
			q = expr();
		}
		else
			q = NULL;
		p = node3(GETLINE, q, p, (NODE *)R_PIN);
	}
  }
  return p;
}

static isincdec(sym)
{
  return sym == INC ? 1 : (sym == DEC ? -1 : 0);
}

static NODE *
expr10()
{
  NODE *p, *q;
  CELL *u, *v;
  int op;
  int c;
int gsave, psave;
  double atof();

/*
printf("expr10(%d)(%s)\n", sym, text);
*/
  switch (sym) {
  case STRING:
	u = mkcell(STR, text, 0.0);
	goto g1;
  case NUMBER:
	u = mkcell(NUM, NULL, atof(text));
g1:
	p = node1(VALUE, u);
	lex();
	break;
  case IDENT: case ARG:
	if ((c = Getc()) == '[') {	/* array */
		/* 940403 */
		if (sym == ARG) {
			u = (CELL *)emalloc(sizeof(CELL));
			u = mkcell(POS, NULL, (double)sym1);
			p = doarray(u);
		}
		else {
			u = getvar(text, hashtab, ARR);
			p = doarray(u);
		}
	}
	else {
		Ungetc(c);
		if (sym == ARG) {
			u = mkcell(POS, NULL, (double)sym1);
			p = node1(ARG, u);
		}
		else {	/* symple variable */
			u = getvar(text, hashtab, VAR|STR|NUM);
			p = node1(VALUE, u);
		}
	}
	lex();
	break;
  case '(':
	/* print >(x ? y : z) needs this */
gsave = getlineflg; psave = printflg;
getlineflg = printflg = 0;
	lex();
	p = expr();
	if (sym == ',')	/* (expr, expr, .. ) */
		p = doelement(p);
	if (sym != ')')
		synerr("')' expected");
getlineflg = gsave; printflg = psave;
	lex();
	break;
  case CALL:
	p = dofuncn(sym, getvar(text, funtab, UDF));
	break;
  case MATHFUN: case STRFUN: case SUBST:
	p = dofuncn(sym, (CELL *)sym1);
	break;
  case SPRINTF:
	p = doprint(FORMAT|STROUT);
	break;
  case '$':
	lex();
	switch (sym) {
	case NUMBER:
		u = mkcell(NUM, NULL, atof(text));
		p = node1(VALUE, u);
		p = node1(FIELD, p);
		lex();
		break;
	case IDENT: case ARG: case '(':
		p = node1(FIELD, expr10());
		break;
	default:
		synerr("number or identifier expected after '$'", (char *)0);
	}
	break;
  case DIV:
	regexflg++;
	lex();
	regexflg = 0;
	u = mkcell(PAT, NULL, 0.0);
	u->c_sval = (char *) mkpat(text);
	p = node1(VALUE, u);
	lex();
	break;
  case GETLINE:
	getlineflg++;
	lex();
	if (sym == IDENT || sym == STRING || sym == ARG)
		q = expr10();	/* read into var */
	else
		q = NULL;
	getlineflg = 0;
	if (sym == R_IN) {
		op = R_IN;
		lex();
		p = expr10();
	}
	else
		op = (int) (p = NULL);
	p = node3(GETLINE, q, p, (NODE *)op);
	break;
  default:
	synerr(
	"identifier, number, string, argument, regexpr, call or '(' expected");
	break;
  }
  return p;
}

static NODE *
dofuncn(fun, op) CELL *op;
{
  NODE *p;
  int i, j;
  int n = 0;
  NODE *a[100];

  if (lex() == '(') {
	prmflg++;
	for (lex(); sym && (sym != ')'); n++) {
		if ((int)op == SPLIT && n == 1) {
/*
printf("sym(%d)sym1(%d)(%d)\n", sym, sym1, isarg(text));
*/
			if (sym != ARG) {	/*isarg(text) == -1*/
				/* make an array if not exist */
				prmflg = 0;
				getvar(text, hashtab, ARR);
				prmflg++;
			}
		}
		a[n] = expr();
		if (sym == ',')
			lex();
		else if (sym != ')')
			synerr("',' or ')' expected");
	}
	prmflg = 0;

	if (sym == ')')
		lex();
	else
		synerr("')' expected");
  }
  p = (NODE *) emalloc(sizeof(*p) + sizeof(p) * (n + 2));
  p->n_type = fun;
  p->n_next = NULL;
  p->n_arg[0] = (NODE *) op;
  p->n_arg[1] = (NODE *) n;
  for (i = 0, j = 2; i < n; )
	p->n_arg[j++] = a[i++];
  p->n_arg[j] = NULL;
  return p;
}

static NODE *
doarray(u) CELL *u;
{
  NODE *p;
  int i, j;
  int n;
  NODE *a[20];

  for (lex(), n = 0; sym &&  sym != ']'; n++) {
	a[n] = expr();
	if (sym == ',')
		lex();
  }
  if (sym != ']')
	synerr("']' expected");
  /* left ']' for expr10() */
  p = (NODE *) emalloc(sizeof(*p) + sizeof(p) * (n + 1));
  p->n_type = ARRAY;
  p->n_next = NULL;
  p->n_arg[0] = (NODE *)u;
  p->n_arg[1] = (NODE *) n;
  for (i = 0, j = 2; i < n; )
	p->n_arg[j++] = a[i++];
  return p;
}

static NODE *
doelement(q) NODE *q;
{
  NODE *p;
  int i, j;
  int n;
  NODE *a[20];

  a[0] = q;
  for (lex(), n = 1; sym &&  sym != ')'; n++) {
	a[n] = expr();
	if (sym == ',')
		lex();
	else if (sym != ')')
		synerr("',' or ')' expected");
  }
  /* left ')' for expr10() */
  p = (NODE *) emalloc(sizeof(*p) + sizeof(p) * (n + 1));
  p->n_type = ELEMENT;
  p->n_next = NULL;
  p->n_arg[0] = NULL;
  p->n_arg[1] = (NODE *) n;
  for (i = 0, j = 2; i < n; )
	p->n_arg[j++] = a[i++];
  return p;
}

synerr(s, t) char *s, *t;
{
  extern int lineno;
  extern char line[], *linep;
  int c, i;
  char *u, *v;

  fprintf(stderr, "%s: Syntax error at line %d", cmd, lineno);
  if (funflg)
	fprintf(stderr, " in function %s", funnam);
  fprintf(stderr, ":\n");
  if ((v = linep - 1) < line)
	v = line + BUFSIZ - 1;
  for (i = 0, u = v - 1; ; --u) {
	if (u < line) {
		if (line[BUFSIZ - 1] == '\0')
			break;
		u = line + BUFSIZ - 1;
	}
	if (*u == '\n' && ++i == 2)
		break;
  }
  if (u != v) {
	while (u != v) {
		fputc(*u, stderr);
		if ((++u - line) == BUFSIZ)
			u = line;
	}
	if (*u != '\n')
		fputc(*u, stderr);
	fprintf(stderr, " <--\n\n");
/*
	fprintf(stderr, " <-- ");
	while ((c = Getc()) != EOF && c != '\n')
		fputc(c, stderr);
	fprintf(stderr, "\n");
	if (c == EOF);
		fprintf(stderr, "\n");
*/
  }
  fprintf(stderr, s, t);
  fprintf(stderr, "\n");
#ifdef DOS
  closeall();
#endif
  exit(1);
}
