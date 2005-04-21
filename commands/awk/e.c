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
#include "regexp.h"

extern char **FS, **OFS, **ORS, **OFMT;
extern double *RSTART, *RLENGTH;
extern char record[];
extern CELL *field[];

extern int r_start, r_length;

double getfval(), atof();
char *strsave(), *getsval(), *strcat(), *strstr();
CELL *mkcell(), *mktmp();
CELL *Field(), *Split(), *Forin();
CELL *Arith(), *Assign(), *Stat(), *Mathfun(), *Strfun(), *Cond();
CELL *Print(), *Cat(), *Array(), *Element();
CELL *If(), *While(), *For(), *Do(), *Jump();
CELL *P1stat(), *P2stat(), *Print0();
CELL *Arg(), *Call(), *Ret();
CELL *Subst(), *In(), *Getline(), *Delete(), *Close();
CELL *Nulproc(), *Usrfun();
CELL *_Arg();

FILE *getfp();	/* r.c */

CELL truecell = { NUM, NULL, 1.0 };
CELL falsecell = { NUM, NULL, 0.0 };
static CELL breakcell = { BRK, NULL, 0.0 };
static CELL contcell = { CNT, NULL, 0.0 };
static CELL nextcell = { NXT, NULL, 0.0 };
static CELL retcell = { RTN, NULL, 0.0 };

static CELL *retval;	/* function return value */

int pateval;	/* used in P1STAT & P2STAT */
static char *r_str;	/* STR in 'str ~ STR */
static regexp *r_pat;	/* compiled pattern for STR */

CELL *(*proctab[])() = {
  Arg, Arith, Array, Assign, Call, Cat, Cond, Delete, Do, Element,
  Field, For, Forin, Getline, If, In, Jump, Mathfun, Nulproc, P1stat,
  P2stat, Print, Print0, Strfun, Subst, Usrfun, While
};

CELL *
execute(p) NODE *p;
{
  int type, i;
  CELL *r, *(*proc)();

  type = p->n_type;
  if (type == VALUE) {
	if ((r = (CELL *) p->n_arg[0])->c_type & PAT && pateval) {
		i = match(r->c_sval, (char *)record) ? 1 : 0;
		r = mktmp(NUM, NULL, (double) i);
	}
	return r;
  }
  for ( ; p != NULL; p = p->n_next) {
#if 0
	if (p->n_type == VALUE) continue;	/* neglect */
#endif
/*
	switch ((int) p->n_type) {
	case ARRAY:
		r = Array(p);
		break;
	case ARITH:
		r = Arith(p);
		break;
	case ASSIGN:
		r = Assign(p);
		break;
	case PRINT:
		r = Print(p);
		break;
	case PRINT0:
		r = Print0(p);
		break;
	case CAT:
		r = Cat(p);
		break;
	case MATHFUN:
		r = Mathfun(p);
		break;
	case STRFUN:
		r = Strfun(p);
		break;
	case COND:
		r = Cond(p);
		break;
	case IF:
		r = If(p);
		break;
	case P1STAT:
		r = P1stat(p);
		break;
	case P2STAT:
		r = P2stat(p);
		break;
	case WHILE:
		r = While(p);
		break;
	case DO:
		r = Do(p);
		break;
	case FOR:
		r = For(p);
		break;
	case FORIN:
		r = Forin(p);
		break;
	case FIELD:
		r = Field(p);
		break;
	case JUMP:
		r = Jump(p);
		break;
	case ARG:
		r = Arg(p);
		break;
	case CALL:
		r = Call(p);
		break;
	case SUBST:
		r = Subst(p);
		break;
	case ELEMENT:
		r = Element(p);
		break;
	case IN:
		r = In(p);
		break;
	case GETLINE:
		r = Getline(p);
		break;
	case DELETE:
		r = Delete(p);
		break;
	case NULPROC:
		r = &truecell;
		break;
	default:
		printf("PROGRAM ERROR ? ILLEGAL NODE TYPE(%d)\n", type);
		exit(1);
		break;
	}
*/
	i = (int) p->n_type;
	if (i < FIRSTP || i > LASTP)
		error("ILLEGAL PROC (%d)", i);
	proc = proctab[i - FIRSTP];
	r = (*proc)(p);
	if (r->c_type & (BRK|CNT|NXT|RTN))
		return r;
	if (p->n_next != NULL)
		c_free(r);
#ifdef DOS
	kbhit();	/* needs in MS-DOS */
#endif
  }
  return r;
}

static CELL *
Arith(p) NODE *p;
{
  int op;
  CELL *r, *u, *v, *execute();
  double x, y, fmod(), pow();

  op = (int) p->n_arg[0];
  if (op == UMINUS) {
	u = execute(p->n_arg[1]);
	x = - getfval(u);
  }
  else if (op == INCDEC) {
	u = execute(p->n_arg[1]);
	x = getfval(u);
	setfval(u, x + (int) p->n_arg[2]);
	if ((int) p->n_arg[3] == PRE)
		return u;
	/* return dummy */
  }
  else {
	u = execute(p->n_arg[1]);
	v = execute(p->n_arg[2]);
	x = getfval(u);
	y = getfval(v);
	if (op == DIV || op == MOD) {
		if (y == 0.0)
			fprintf(stderr, "divid by 0\n");
	}
	switch (op) {
	case SUB: x -= y;break;
	case ADD: x += y; break;
	case MULT: x *= y; break;
	case DIV:
		if (y == 0.0)
			error("division by zero in \"/\"", (char *)0);
		x /= y; break;
	case MOD:
		if (y == 0.0)
			error("division by zero in \"%%\"", (char *)0);
		x = fmod(x, y); break;
	case POWER: x = pow(x, y); break;
	default: printf("UNSUPPORTED ARITH OPERATOR !\n"); break;
	}
	c_free(v);
  }
  c_free(u);
  r = mktmp(NUM, NULL, x);
  return r;
}

static CELL *
Assign(p) NODE *p;
{
  CELL *u, *v, *execute();
  int op;
  double x, y, fmod(), pow();

  op = (int) p->n_arg[0];
  u = execute(p->n_arg[1]);

#if 0
  if (u->c_type == UDF)	/* fix up local var */
	u->c_type |= VAR|STR;
#endif
  if (!(u->c_type & (VAR|FLD|REC)) && (u->c_type != UDF))
	fprintf(stderr, "ASSIGN TO NON VARIABLE (%d)\n", u->c_type);
  v = execute(p->n_arg[2]);

  if (u == v)
	goto rtn;	/* same node */

  if (op == ASSIGN) {
	if (v->c_type & NUM/* || isnum(v->c_sval)*/)
		setfval(u, getfval(v));
	else
		setsval(u, getsval(v));
  }
  else {
	x = getfval(u);
	y = getfval(v);
	switch (op) {
	case ADDEQ:	x += y; break;
	case SUBEQ:	x -= y; break;
	case MULTEQ:	x *= y; break;
	case DIVEQ:
		if (y == 0.0)
			error("division by zero in \"/=\"", (char *)0);
		x /= y; break;
	case MODEQ:
		if (y == 0.0)
			error("division by zero in \"%=\"", (char *)0);
		x = fmod(x, y); break;
	case POWEQ:	x = pow(x, y); break;
	default:
		synerr("illegal assign op (%d)", op);
		break;
	}
	setfval(u, x);
  }
rtn:
  c_free(v);
  return u;
}

static CELL *
Cat(p) NODE *p;
{
  CELL *u;
  char *s, *t, str[BUFSIZ];

  u = execute(p->n_arg[0]);
  s = getsval(u);
  for (t = str; *s; )
	*t++ = *s++;
  c_free(u);
  u = execute(p->n_arg[1]);
  s = getsval(u);
  while (*s)
	*t++ = *s++;
  c_free(u);
  *t = '\0';
 return mktmp(STR, str, 0.0);
}

static CELL *
Print(p) NODE *p;
{
  register int i, redir, typ;
  CELL *u;
  char *s, str[BUFSIZ];
  char *file;
  FILE *fp;

  redir = (int) p->n_arg[0];
  if (typ = redir & PRMASK) {	/* redirect */
	u = execute(p->n_arg[1]);
	file = getsval(u);
	if (typ == R_PIPE)
		typ = R_POUT;
	fp = getfp(file, typ);
	c_free(u);
  }
  else
	fp = stdout;
  if (redir & FORMAT)	/* format */
	format(str, p);
  else {
	*str = '\0';
	for (i = 2; p->n_arg[i] != NULL; i++) {
		if (i > 2)
			strcat(str, *OFS);
		u = execute(p->n_arg[i]);
		s = getsval(u);
		strcat(str, s);
		c_free(u);
	}
	strcat(str, *ORS);
  }
  if (redir & STROUT)	/* sprintf */
	return mktmp(STR, str, 0.0);
  fputs(str, fp);
  fflush(fp);
  return &truecell;
}

static CELL *
Mathfun(p) NODE *p;
{
  CELL *u, *v;
  double x, y;
  double atan2(), cos(), exp(), log(), sin(), sqrt(), modf();

  if ((int) p->n_arg[1] == 0) {
	u = NULL;
	x = 0.0;
  }
  else {
	u = execute(p->n_arg[2]);
	x = getfval(u);
  }
  switch ((int) p->n_arg[0]) {
  case ATAN2:
	if ((int) p->n_arg[1] == 2) {
		v = execute(p->n_arg[3]);
		y = getfval(v);
		x = atan2(x, y);
		c_free(v);
	}
	else
		x = 0.0;
	break;
  case COS:	x = cos(x); break;
  case EXP:	x = exp(x); break;
  case INT:	y = modf(x, &x); break;
  case LOG:	x = log(x); break;
  case SIN:	x = sin(x); break;
  case SQRT:	x = sqrt(x); break;
  case RAND:	x = (double) rand() / 32768.0; break;
  case SRAND:	if (x == 0.0)
			x = (double) time(0);
		x = (double) srand((int) x);
		break;
  default:
	fprintf(stderr, "unknown math function (%d)\n", p->n_arg[2]);
	break;
  }
  if (u != NULL)
	c_free(u);
  return mktmp(NUM, NULL, x);
}

static CELL *
Strfun(p) NODE *p;
{
  CELL *u, *v, *r;
  char *s, *t, str[BUFSIZ];
  int i, m, n;
  double x;
  regexp *pat, *getpat();

  n = (int) p->n_arg[1];
  if (n > 0 && (int) p->n_arg[0] != SPLIT) {
	u = execute(p->n_arg[2]);
	s = getsval(u);
  }
  else {
	s = "";
	u = NULL;
  }
  switch ((int) p->n_arg[0]) {
  case INDEX:
	if (n > 1) {
		v = execute(p->n_arg[3]);
		t = getsval(v);
		i = Index(s, t);
		c_free(v);
	}
	else
		i = 0;
	r = mktmp(NUM, NULL, (double) i);
	break;
  case LENGTH:
	i = (n > 0) ? jstrlen(s) : jstrlen(record);
	r = mktmp(NUM, NULL, (double) i);
	break;
  case SPLIT:
	r = Split(p);
	break;
  case SUBSTR:
	if (n > 1) {
		v = execute(p->n_arg[3]);
		m = (int) getfval(v) - 1;
		c_free(v);
	}
	else
		m = 0;
	if (n > 2) {
		v = execute(p->n_arg[4]);
		n = (int) getfval(v);
		c_free(v);
	}
	else
		n = jstrlen(s) - m;
	for (t = str; *s && m-- > 0; s++)
		if (isKanji(*s))
			s++;
	while (*s && n-- > 0) {
		if (isKanji(*s))
			*t++ = *s++;
		*t++ = *s++;
	}
	*t = '\0';
	r = mktmp(STR, str, 0.0);
	break;
  case RMATCH:
	if (n > 1) {
		v = execute(p->n_arg[3]);
		pat = getpat(v);
		match(pat, s);
		c_free(v);
		if (r_start) {	/* change only if match */
			*RSTART = (double) r_start;
			*RLENGTH = (double) r_length;
		}
		r = mktmp(NUM, NULL, (double) r_start);
	}
	else
		error("missing regexpr in match(str, regexpr)");
	break;
  case CLOSE:
	r = Close(s);
	break;
  case SYSTEM:
	r = mktmp(NUM, NULL, system(s) == -1 ? 0.0 : 1.0);
	break;
  default:
	fprintf(stderr, "unknown string function");
	break;
  }
  c_free(u);
  return r;
}

static regexp *
getpat(r) CELL *r;
{
  regexp *pat, *mkpat();

  if (r->c_type & PAT)
	pat = (regexp *) r->c_sval;
  else {
	if (r_str && strcmp(r_str, r->c_sval) == 0)
		pat = r_pat;
	else {
		sfree(r_str); sfree(r_pat);
		r_str = strsave(getsval(r));
		pat = r_pat = mkpat(r_str);
	}
  }
  return pat;
}

static CELL *
Subst(p) NODE *p;
{
  CELL *u, *v, *w;
  char *s, *t, *r, str[BUFSIZ], *strcpy();
  int i, n;

  n = (int) p->n_arg[1];
  if (n > 1) {
	u = execute(p->n_arg[3]);	/* substitute string */
	s = getsval(u);
	v = execute(p->n_arg[2]);	/* expr */
	if (n > 2) {
		w = execute(p->n_arg[4]);
		t = getsval(w);
		r = str;
	}
	else {
		t = r = record;
		w = NULL;
	}
	i = (int) p->n_arg[0] == RGSUB ? 0 : 1;
	if (v->c_type & (PAT|STR))
		i = Sub(r, v->c_sval, (v->c_type & STR), s, t, i);
	else
		error("[g]sub(PAT, .. ) must be /../ or string (%d)",
			w->c_type);
	if (n > 2) {
		if (w->c_type & REC) {
			strcpy(record, str);
			mkfld(record, *FS, field);
		}
		else
			setsval(w, str);
	}
	else
		mkfld(record, *FS, field);
	c_free(u);
	c_free(v);
	c_free(w);
  }
  else
	i = 0;
  return mktmp(NUM, NULL, (double) i);
}

static CELL *
Cond(p) NODE *p;
{
  CELL *u, *v;
  double x, y;
  int op, i, j;
  char *s;
  int save = pateval;

  op = (int) p->n_arg[0];
  u = execute(p->n_arg[1]);
  x = getfval(u);
/*
printf("Cond(%d)(%s)\n", u->c_type, u->c_sval);
*/
  if (op == AND || op == OR || op == NOT) {
	if (u->c_type & NUM)
		i = (x != 0.0);
	else {
		s = getsval(u);
		i = (s != (char *)NULL) && (*s != '\0');
	}
  }
  if (op == AND && !i) {
	c_free(u);
	return &falsecell;
  }
  if (op == OR && i) {
	c_free(u);
	return &truecell;
  }
  if (op == NOT)
	i = i == 0 ? 1 : 0;
  else {
	if (op == MATCH || op == NOMATCH)
		pateval = 0;
	v = execute(p->n_arg[2]);
	y = getfval(v);
	if (op == AND || op == OR || op == BINAND || op == BINOR) {
		if (v->c_type & NUM)
			j = (y != 0.0);
		else {
			s = getsval(v);
			j = (s != (char *)NULL) && (*s != '\0');
		}
		switch (op) {
		case AND:	i = i && j; break;
		case OR:	i = i || j; break;
		case BINAND:	i = i & j; break;
		case BINOR:	i = i | j; break;
		}
	}
	else if (op == MATCH || op == NOMATCH) {
		char *s;
		regexp *pat, *getpat();

		s = getsval(u);
		pat = getpat(v);
		i = match(pat, s) == 0 ? 0 : 1;
		if (op == NOMATCH)
			i = i == 0 ? 1 : 0;
	}
	else {	/* relative operator */
/*
printf("Cond(%d)(%d)(%s)(%s)\n", u->c_type, v->c_type, u->c_sval, v->c_sval);
*/
		if ((u->c_type & NUM) && (v->c_type & NUM))
			i = x < y ? -1 : (x > y ? 1 : 0);
		else
			i = strcmp(getsval(u), getsval(v));
/*
printf("Cond(%d)(%d)(%g)(%g)(%d)\n", u->c_type, v->c_type, x, y, i);
*/

		switch (op) {
		case LT:	i = i < 0 ? 1 : 0; break;
		case LE:	i = i <= 0 ? 1 : 0; break;
		case EQ:	i = i == 0 ? 1 : 0; break;
		case NE:	i = i != 0 ? 1 : 0; break;
		case GT:	i = i > 0 ? 1 : 0; break;
		case GE:	i = i >= 0 ? 1 : 0; break;
		default:
			fprintf(stderr, "unknown relative operator (%d)\n", op);
			break;
		}
	}
	c_free(v);
  }
  c_free(u);
  pateval = save;
  return mktmp(NUM, NULL, (double) i);
}

static CELL *
If(p) NODE *p;
{
  CELL *u;
  int i;
  char *s;

  u = execute(p->n_arg[0]);
  if (u->c_type & NUM)
	i = (getfval(u) != 0.0);
  else {
	s = getsval(u);
	i = (s != (char *)NULL) && (*s != '\0');
  }
  c_free(u);
  if (i)
	u = execute(p->n_arg[1]);
  else if (p->n_arg[2])
	u = execute(p->n_arg[2]);
  else
	u = &truecell;
  return u;
}

static CELL *
While(p) NODE *p;
{
  CELL *u;
  double x;

  for (;;) {
	u = execute(p->n_arg[0]);
	x = getfval(u);
	if (x == 0.0)
		break;
	c_free(u);
	u = execute(p->n_arg[1]);
	switch (u->c_type) {
	case BRK:
		goto rtn;
	case NXT: case EXT: case RTN:
		return u;
	}
	c_free(u);
  }
rtn:
  c_free(u);
  return &truecell;
}

static CELL *
Do(p) NODE *p;
{
  CELL *u;
  double x;

  for (;;) {
	u = execute(p->n_arg[0]);
	switch (u->c_type) {
	case BRK:
		goto rtn;
	case NXT: case EXT: case RTN:
		return u;
	}
	c_free(u);
	u = execute(p->n_arg[1]);
	if(getfval(u) == 0.0)
		break;
	c_free(u);
  }
rtn:
  c_free(u);
  return &truecell;
}

static CELL *
For(p) NODE *p;
{
  CELL *u;
  double x;

  if (p->n_arg[0] != NULL) {
	u = execute(p->n_arg[0]);
	c_free(u);
  }
  for (;;) {
	if (p->n_arg[1] != NULL) {
		u = execute(p->n_arg[1]);
		x = getfval(u);
		c_free(u);
		if (x == 0.0)
			break;
	}
	u = execute(p->n_arg[3]);
	switch (u->c_type) {
	case BRK:
		c_free(u);
		goto rtn;
	case NXT: case EXT: case RTN:
		return u;
	}
	if (p->n_arg[2] != NULL) {
		u = execute(p->n_arg[2]);
		c_free(u);
	}
  }
rtn:
  return &truecell;
}

static CELL *
Jump(p) NODE *p;
{
  CELL *u;
  int i;

  switch ((int) p->n_arg[0]) {
  case BREAK:	u = &breakcell; break;
  case CONTIN:	u = &contcell;  break;
  case EXIT:
	if ((int) p->n_arg[1]) {
		u = execute(p->n_arg[1]);
		i = (int) getfval(u);
	}
	else
		i = 0;
	closeall();
	exit(i);
  case RETURN:
	Return(p);
	u = &retcell;
	break;
  case NEXT:	u = &nextcell; break;
  }
  return u;
}

static
Return(p) NODE *p;
{
  CELL *u;
  int i;
  char *s, str[BUFSIZ];

  c_free(retval);
  if (p->n_arg[1] != NULL) {
	if (p->n_arg[2] == NULL) {
/*
if (0) {
*/
		u = execute(p->n_arg[1]);
		if (u->c_type == UDF)
			retval = mktmp(STR, "", 0.0);
		else
			retval = mktmp(u->c_type, u->c_sval, u->c_fval);
		c_free(u);
	}
	else {
		for (i = 1; p->n_arg[i] != NULL; i++) {
			if (i == 1)
				*str = '\0';
			else
				strcat(str, *OFS);
			u = execute(p->n_arg[i]);
			s = getsval(u);
			strcat(str, s);
			c_free(u);
		}
/*
printf("Ret(%s)(%d)\n", str, isnum(str));
*/
		if (isnum(str))
			retval = mktmp(STR|NUM, str, atof(str));
		else
			retval = mktmp(STR, str, 0.0);
	}
  }
  else
	retval = &truecell;
}

#define MAXFRAME	100
CELL **frame[MAXFRAME];
static int framep;

static CELL *
Arg(p) NODE *p;
{
  CELL *u;
  int i;

  u = (CELL *)p->n_arg[0];
  return _Arg((int)u->c_fval);
}

CELL *
_Arg(i)
{
/*
printf("Arg(%d)\n", i);
*/
  return frame[framep - 1][i];
}

static CELL *
Call(p) NODE *p;
{
  CELL *u, *v, *r, **arg;
  NODE *q;
  int i, j, k, n;
  char *emalloc();

  if (framep >= MAXFRAME - 2)
	error("stack frame overflow", (char *)0);
  retval = &truecell;
  r = (CELL *) p->n_arg[0];
  if (r->c_type != FUN)
	synerr("called function is not declared", (char *)0);
  n = (int) r->c_fval;	/* # of params */
  if (n > 0) {
	arg = (CELL **) emalloc(sizeof(u) * n);
	for (i = 2, j = 0, k = (int) p->n_arg[1]; j < k; i++) {
		u = execute(p->n_arg[i]);
/*
printf("pass, j(%d)typ(%d)\n", j, u->c_type);
*/
		if (u->c_type & ARR)
			v = u;	/* pass by reference */
		else {	/* pass by value */
			v = mkcell(UDF, u->c_sval, u->c_fval);
			if (u->c_type != UDF) {
#if 0
				v->c_type = u->c_type;
				if (v->c_type & (NUM|STR))
					v->c_type |= VAR;
				v->c_type &= ~TMP;	/* dont't free */
#else
				v->c_type |= (u->c_type & (NUM|STR))|VAR;
				/*v->c_type &= ~TMP;*/
#endif
				/* Don't free original */
			}
/*
printf("pass1, j(%d)typ(%d)\n", j, v->c_type);
*/
		}
		arg[j++] = v;
	}
	for ( ; j < n; )	/* local var */
		arg[j++] = mkcell(UDF, NULL, 0.0);
  }
  else
	arg = NULL;

  frame[framep] = arg;
  framep++;

  r = execute(r->c_sval);
  c_free(r);
  framep--;
  if (n > 0) {
	for (j = n - 1 ; j > k; j--) {	/* local var */
		u = arg[j];
		if (u->c_type & ARR)
			a_free(u);
		else
			c_free(u);
	}
	for ( ; j >= 0; j--) {
		u = arg[j];
		if (!(u->c_type & ARR)) {
/*			c_free(u);*/
			sfree(u->c_sval);
			sfree(u);
		}
		else {
			v = execute(p->n_arg[j + 2]);
			if (v->c_type == UDF) {	/* copy back */
/*
printf("copy_back_UDF(%d)(%d)\n", j, u->c_type);
*/
				v->c_type = u->c_type;
				sfree(v->c_sval);
				v->c_sval = u->c_sval;
				v->c_fval = u->c_fval;
				sfree(u);
			}
		}
	}
  }
  sfree(arg);
/*  return retval;*/
  u = mktmp(retval->c_type, retval->c_sval, retval->c_fval);
  return u;
}

CELL *Nulproc()
{
  return &truecell;
}

CELL *
Usrfun(p) NODE *p;
{
  CELL *u;

  u = execute(p);
  return u;
}
