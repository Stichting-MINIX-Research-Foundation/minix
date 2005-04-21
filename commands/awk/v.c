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

#define PI	3.14159265358979323846

#define HASHSIZE	50
#define MAXFIELD	100

double atof();
char *getsval(), *jStrchar();
extern CELL *execute(), *_Arg();

extern char record[];
extern CELL *field[];

extern CELL truecell, falsecell;
extern prmflg;

SYMBOL *hashtab[HASHSIZE];
SYMBOL *funtab[HASHSIZE];
SYMBOL *argtab[HASHSIZE];

char *strsave(), *emalloc(), *strchr();
CELL *lookup(), *install(), *_install(), *mkcell(), *mktmp(), *getvar();

char **FS, **RS, **OFS, **ORS, **OFMT, **FILENAME;
char **SUBSEP;
double *NR, *NF;
double *FNR, *ARGC, *RSTART, *RLENGTH;

init()
{
  FS = &install("FS", VAR|STR, " ", 0.0, hashtab)->c_sval;
  RS = &install("RS", VAR|STR, "\n", 0.0, hashtab)->c_sval;
  OFS = &install("OFS", VAR|STR , " ", 0.0, hashtab)->c_sval;
  ORS = &install("ORS", VAR|STR, "\n", 0.0, hashtab)->c_sval;
  OFMT = &install("OFMT", VAR|STR, "%.6g", 0.0, hashtab)->c_sval;
  NR = &install("NR", VAR|NUM, (char *)NULL, 0.0, hashtab)->c_fval;
  NF = &install("NF", VAR|NUM, (char *)NULL, 0.0, hashtab)->c_fval;
  FILENAME = &install("FILENAME", VAR|STR, (char *)NULL, 0.0, hashtab)->c_sval;
  install("PI", VAR|NUM, (char *)NULL, PI, hashtab);
  field[0] = mkcell(REC|STR, (char *)NULL, 0.0);	/* $0 */
  field[0]->c_sval = record;
  SUBSEP = &install("SUBSEP", VAR|STR, "\034", 0.0, hashtab)->c_sval;
  FNR = &install("FNR", VAR|NUM, (char *)NULL, 0.0, hashtab)->c_fval;
  RSTART = &install("RSTART", VAR|NUM, (char *)NULL, 0.0, hashtab)->c_fval;
  RLENGTH = &install("RLENGTH", VAR|NUM, (char *)NULL, 0.0, hashtab)->c_fval;
}

setvar(s) char *s;
{
  CELL *u;
  char *t;

  for (t = s; *t && *t != '='; t++)
	;
  *t++ = '\0';
  if ((u = lookup(s, hashtab)) == (CELL *)NULL) {
	if (isnum(t))
		install(s, VAR|NUM|STR, t, atof(t), hashtab);
	else
		install(s, VAR|STR, t, 0.0, hashtab);
  }
  else {
	if (isnum(t))
		setfval(u, atof(t));
	else
		setsval(u, t);
  }
}

initarg(arg0, argc, argv) char *arg0, **argv;
{
  CELL *u;
  register int i;
  register char str[4];

  ARGC = &install("ARGC", VAR|NUM, (char *)NULL, (double)argc+1, hashtab)->c_fval;
  u = install("ARGV", ARR, (char *)NULL, 0.0, hashtab);
  u->c_sval = (char *) argtab;
  install("0", VAR|STR, arg0, 0.0, argtab);
  for (i = 0; i < argc; i++) {
	sprintf(str, "%d", i+1);
	if (isnum(argv[i]))
		install(str, VAR|STR|NUM, argv[i], atof(argv[i]), argtab);
	else
		install(str, VAR|STR, argv[i], 0.0, argtab);
  }
}

static
hash(s) unsigned char *s;
{
  register unsigned int h;

  for (h = 0; *s; )
	h += *s++;
  return h % HASHSIZE;
}

CELL *
lookup(s, h) char *s; SYMBOL *h[];
{
  register SYMBOL *p;

  for (p = h[hash(s)]; p; p = p->s_next)
	if (strcmp(s, p->s_name) == 0)
		return p->s_val;
  return (CELL *)NULL;
}

static CELL *
install(name, type, sval, fval, h) char *name, *sval; double fval; SYMBOL *h[];
{
  CELL *u;

  if ((u = lookup(name, h)) == (CELL *)NULL)
	u = _install(name, type, sval, fval, h);
  else
	error("%s is doubly defined", name);
  return u;
}

static CELL *
_install(name, type, sval, fval, h) char *name, *sval; double fval; SYMBOL *h[];{
  register SYMBOL *p;
  CELL *u;
  int hval;

  p = (SYMBOL *) emalloc(sizeof(*p));
  u = (CELL *) emalloc(sizeof(*u));
  p->s_name = strsave(name);
  p->s_val = u;
  hval = hash(name);
  p->s_next = h[hval];
  h[hval] = p;
  u->c_type = type;
  u->c_sval = strsave(sval);
#if 0
  if (!(type & NUM) && isnum(sval)) {
	u->c_fval = atof(sval);
	u->c_type |= NUM;
  }
  else
#endif
	u->c_fval = fval;
  return u;
}

CELL *
getvar(s, h, typ) char *s; SYMBOL *h[];
{
  CELL *u;
  SYMBOL *p;
  char *t;
  int i, hval;

  if ((u = lookup(s, h)) == (CELL *)NULL) {
	if (prmflg) {
		u = _install(s, UDF, "", 0.0, h);
		goto rtn;
	}
	else if (typ & ARR) {
		t = emalloc(sizeof(SYMBOL *) * HASHSIZE);
		for (i = 0; i < HASHSIZE; i++)
			((SYMBOL **) t)[i] = (SYMBOL *)NULL;
		u = (CELL *) emalloc(sizeof(*u));
		u->c_type = typ;
		u->c_sval = t;
		u->c_fval = 0.0;
		p = (SYMBOL *) emalloc(sizeof(*p));
		p->s_name = strsave(s);
		p->s_val = u;
		hval = hash(s);
		p->s_next = h[hval];
		h[hval] = p;
	}
	else
		u = _install(s, typ, "", 0.0, h);
  }
  else if (!prmflg && (u->c_type == UDF) && (typ != UDF)) {
	/* fix up local_var/forward_function */
	if (typ == ARR) {
/*
printf("getvar_correct_to_array\n");
*/
		u->c_type = typ;
		sfree(u->c_sval);
		u->c_sval = emalloc(sizeof(SYMBOL *) * HASHSIZE);
		for (i = 0; i < HASHSIZE; i++)
			((SYMBOL **) u->c_sval)[i] = (SYMBOL *)NULL;
		u->c_fval = 0.0;
	}
	else if (typ != UDF) {
		u->c_type = typ;
	}
 }
rtn:
  return u;
}

fixarray(u) CELL *u;
{
  int i;

  if (u->c_type == UDF) {	/* fix up local var */
/*
printf("fixarray\n");
*/
	u->c_type = ARR;
	sfree(u->c_sval);
	u->c_sval = emalloc(sizeof(SYMBOL *) * HASHSIZE);
	for (i = 0; i < HASHSIZE; i++)
		((SYMBOL **) u->c_sval)[i] = (SYMBOL *)NULL;
	u->c_fval = 0.0;
  }
}

a_free(u) CELL *u;
{	/* free local array */
  SYMBOL **h, *q, *r;
  CELL *v;
  int i;

  if (!(u->c_type & ARR))
	error("try to free non array variable", (char *)0);
  h = (SYMBOL **) u->c_sval;
  for (i = 0; i < HASHSIZE; i++)
	for (q = h[i]; q; q = r) {
		r = q->s_next;
		sfree(q->s_name);
		v = q->s_val;	/* CELL */
		c_free(v);
		sfree(q);	/* SYMBOL */
	}

  sfree(u->c_sval);	/* symbol table */
  c_free(u);
}

CELL *
Array(p) NODE *p;
{
  CELL *u;
  char str[BUFSIZ];
  int i, n;

  CELL *v;

  u = (CELL *) p->n_arg[0];
  if (u->c_type == POS) {
	i = (int)u->c_fval;
/*
printf("**ARG_ARRAY(%d)*\n", i);
*/
	u = _Arg(i);
	if (u->c_type == UDF) {	/* fix up local array */
/*
printf("local_var_to_array\n");
*/
		fixarray(u);
	}
  }
  else if (!(u->c_type & ARR))
	error("non array refference");
  arrayelm(p, str);
  u = getvar(str, u->c_sval, VAR|NUM|STR);	/* "rtsort in AWK book */
  return u;
}

static
arrayelm(p, s) NODE *p; char *s;
{
  CELL *u;
  int i, n;
  char *t;

/*
char *tt = s;
*/
  n = (int) p->n_arg[1] + 2;
  for (i = 2; i < n; i++) {
	if (i > 2)
		*s++ = **SUBSEP;
	u = execute(p->n_arg[i]);
	for (t = getsval(u); *t; )
		*s++ = *t++;
	c_free(u);
  }
  *s = '\0';
/*
printf("array_elm(%s)\n", tt);
*/
}

CELL *
Element(p) NODE *p;
{
  char str[BUFSIZ];

  arrayelm(p, str);
  return mktmp(STR, str, 0.0);
}

CELL *
Delete(p) NODE *p;
{
  CELL *u;
  char str[BUFSIZ];
  int i;
  SYMBOL *q, *r, **h;

  u = (CELL *) p->n_arg[0];
  if (!(u->c_type & ARR))
	error("can't delete non array variable");
  arrayelm(p, str);
  h = (SYMBOL **) u->c_sval;
  for (r = (SYMBOL *)NULL, i = hash(str), q = h[i]; q; r = q, q = q->s_next)
	if (strcmp(str, q->s_name) == 0)
		break;
  if (q) {
	sfree(q->s_val->c_sval);
	sfree(q->s_name);
	if (r)
		r->s_next = q->s_next;
	if (q == h[i])
		h[i] = (SYMBOL *)NULL;
  }
  return &truecell;
}

CELL *
In(p) NODE *p;
{
  SYMBOL **h, *q;
  CELL *u, *v;
  char *s;
  int i;

  u = (CELL *) p->n_arg[1];	/* array */
  if (!(u->c_type & ARR))
	error("%s is not an array", u->c_sval);
  h = (SYMBOL **) u->c_sval;
  if (u->c_sval != (char *)NULL) {
	v = execute(p->n_arg[0]);	/* var */
	s = getsval(v);
	for (i = 0; i < HASHSIZE; i++)
		for (q = h[i]; q; q = q->s_next) {
			if (strcmp(s, q->s_name) == 0) {
				c_free(v);
				return &truecell;
			}
		}
	c_free(v);
  }
  return &falsecell;
}

CELL *
Split(p) NODE *p;
{
  CELL *u, *v, *w;
  char *s, *t, *h, *name, *sep;
  int i, n, skip;
  char elm[8], str[BUFSIZ];
  static char *s_str;
  static regexp *s_pat;
  regexp *mkpat();
  extern int r_start, r_length;

  n = (int) p->n_arg[1];
  if (n > 1) {
	u = execute(p->n_arg[2]);
	s = getsval(u);			/* str */
	v = execute(p->n_arg[3]);	/* array */
	if (!(v->c_type & ARR)) {
/*
printf("Split fix_to_array(%d)\n", v->c_type);
*/
		if (v->c_type == UDF)	/* fix up local array */
			fixarray(v);
		else
			error("split to non array variable", (char *)0);
	}
	h = v->c_sval;
	c_free(v);
	if (n > 2) {
		v = execute(p->n_arg[4]);
		sep = getsval(v);
	}
	else {
		v = (CELL *)NULL;
		sep = *FS;
	}
	if (strlen(sep) > 1) {	/* reg_exp */
		if (strcmp(sep, s_str) != 0) {
			sfree(s_str); sfree(s_pat);
			s_str = strsave(sep);
			s_pat = mkpat(s_str);
		}
		for (i = 0, t = str; *s; ) {
			if (match(s_pat, s)) {
				for (n = r_start; --n > 0; )
					*t++ = *s++;
			}
			else {
				while(*s)
					*t++ = *s++;
			}
			*t = '\0';
			t = str;
			sprintf(elm, "%d", ++i);
			w = getvar(elm, h, VAR);
			if (isnum(str))
				setfval(w, atof(str));
			else
				setsval(w, str);
			if (*s)
				s += r_length;
		}
	}
	else {
		skip = *sep == ' ';
		for (i = 0; t = str, *s; ) {
			if (skip)
				while (jStrchr(" \t\n", *s))
					s++;
			if (!(*s))
				break;
			while (*s && !jStrchr(sep, *s)) {
				if (isKanji(*s))
					*t++ = *s++;
				*t++ = *s++;
			}
			*t = '\0';
			sprintf(elm, "%d", ++i);
			w = getvar(elm, h, VAR);
			if (isnum(str))
				setfval(w, atof(str));
			else
				setsval(w, str);
			if (*s && !skip)
				s++;
		}
	}
	c_free(v);	/* sep */
	c_free(u);	/* str may be CATed */
  }
  else
	i = 0;
  return mktmp(NUM, (char *)NULL, (double) i);
}

CELL *
Forin(p) NODE *p;
{
  CELL *u, *v;
  SYMBOL **h, *q;
  char *name;
  int i;

  u = execute(p->n_arg[1]);
  if (!(u->c_type & ARR))
	synerr(
	"non array variable is specified in 'for (. in var)'", (char *)0);
  h = (SYMBOL **) u->c_sval;
  c_free(u);
  u = execute(p->n_arg[0]);
  if (u->c_type == UDF) {
/*
printf("Forin_fix_to_VAR|NUM\n");
*/
	u->c_type = VAR|NUM;
  }
  if (!(u->c_type & VAR))
	error("'for (VAR in .)' is not variable (%d)", name, u->c_type);
  for (i = 0; i < HASHSIZE; i++) {
	for (q = h[i]; q; q = q->s_next) {
		setsval(u, q->s_name);
		v = execute(p->n_arg[2]);
		c_free(v);
	}
  }
  c_free(u);
  return &truecell;
}

char *
strsave(s) char *s;
{
  register int n;
  char *emalloc(), *strcpy();

  if (s == (char *)NULL)
	return (char *)NULL;
  n = strlen(s) + 1;
  return strcpy(emalloc(n), s);
}

sfree(p) char *p;
{
  if (p != (char *)NULL)
	Free(p);
}

isnum(s) char *s;
{
  char *strchr();

  if (s == NULL || *s == '\0' || !strcmp(s, "."))
	return 0;
  if (*s && strchr("+-", *s) != (char *)NULL)
	s++;
  if (*s == '\0')
	return 0;
  while (isdigit(*s))
	s++;
  if (*s == '.') {
	s++;
	while (isdigit(*s))
		s++;
  }
  if (*s && strchr("eE", *s) != (char *)NULL) {
	s++;
	if (*s == '\0')
		return 0;
	if (*s && strchr("+-", *s) != (char *)NULL)
		s++;
	while (isdigit(*s))
		s++;
  }
  return *s == '\0';
}

setfval(u, f) CELL *u; double f;
{
  if (u->c_type == UDF) {	/* fix up local var */
/*
printf("setfval_fix_to_VAR\n");
*/
	u->c_type |= VAR;
  }
  if (u->c_type & (VAR|FLD|REC|TMP)) {
	u->c_type &= ~STR;
	u->c_type |= NUM;
	sfree(u->c_sval);
	u->c_sval = (char *)NULL;
	u->c_fval = f;
	if (u->c_type & FLD)
		mkrec(u);
  }
  else
	fprintf(stderr, "assign to nonvariable (%d)\n", u->c_type);
}

setsval(u, s) CELL *u; char *s;
{
  double atof();

  if (u->c_type == UDF) {	/* fix up local var */
/*
printf("setsval_fix_to_VAR\n");
*/
	u->c_type |= VAR;
  }
  if (u->c_type & (VAR|FLD|REC|TMP)) {
	u->c_type &= ~NUM;
	u->c_type |= STR;
	sfree(u->c_sval);
	u->c_sval = strsave(s);
#if 0	/* "table2" in AWK book */
	if (isnum(u->c_sval)) {
		u->c_fval = atof(u->c_sval);
		u->c_type |= NUM;
	}
	else
#endif
		u->c_fval = 0.0;
	if (u->c_type & FLD)
		mkrec(u);
  }
  else
	fprintf(stderr, "assign to constant (%d)\n", u->c_type);
}

double
getfval(u) CELL *u;
{
  double x, atof();

  if (u->c_type == UDF) {	/* local var */
	u->c_type |= VAR|STR|NUM;
	u->c_sval = strsave("");
	x = u->c_fval = 0.0;
  }
  else if (u->c_type & NUM)
	x = u->c_fval;
#if 1
  else {
	x = atof(u->c_sval);
#else
  else {
	if (isnum(u->c_sval))
		x = atof(u->c_sval);
	else
		x = 0.0;
#endif
  }
  return x;
}

char *
getsval(u) CELL *u;
{
  char *s, str[80];

  if (u->c_type & STR)
	s = u->c_sval;
  else if (u->c_type & NUM) {
/*	if (u->c_fval >= -2147483648.0 && u->c_fval <= 2147483647.0)*/
	if ((long)u->c_fval == u->c_fval)
		s = "%.16g";
	else
		s = *OFMT;
	sprintf(str, s, u->c_fval);
	sfree(u->c_sval);
	s = u->c_sval = strsave(str);
  }
#if 1
  else if (u->c_type == UDF) {	/* local var */
/*
printf("getsval_fix_to_VAR|STR\n");
*/
	u->c_type |= VAR|STR|NUM;
	s = u->c_sval = strsave("");
	u->c_fval = 0.0;
  }
#endif
  else
	fprintf(stderr, "abnormal value (STR|NUM == 0)(%d)\n", u->c_type);
  return s;
}

char *
emalloc(n) unsigned n;
{
  char *p;
#if 0
  char far *_fmalloc();
#else
  char *malloc();
#endif

#if 0
  if ((p = _fmalloc(n)) == (char *)NULL)
#else
  if ((p = malloc(n)) == (char *)NULL)
#endif
	error("memory over");
  return p;
}

Free(s) char *s;
{
#if DOS
  void _ffree();

  _ffree(s);
#else
  free(s);
#endif
}
