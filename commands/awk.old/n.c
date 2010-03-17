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

NODE *
node0(type)
{
  NODE *p;
  char *emalloc();

  p = (NODE *) emalloc(sizeof(*p) - sizeof(p));
  p->n_type = type;
  p->n_next = NULL;
  return p;
}

NODE *
node1(type, arg0) NODE *arg0;
{
  NODE *p;
  char *emalloc();

  p = (NODE *) emalloc(sizeof(*p));
  p->n_type = type;
  p->n_next = NULL;
  p->n_arg[0] = (NODE *) arg0;
  return p;
}

NODE *
node2(type, arg0, arg1) NODE *arg0, *arg1;
{
  NODE *p;
  char *emalloc();

  p = (NODE *) emalloc(sizeof(*p) + sizeof(p) * 1);
  p->n_type = type;
  p->n_next = NULL;
  p->n_arg[0] = (NODE *) arg0;
  p->n_arg[1] = (NODE *) arg1;
  return p;
}

NODE *
node3(type, arg0, arg1, arg2) NODE *arg0, *arg1, *arg2;
{
  NODE *p;
  char *emalloc();

  p = (NODE *) emalloc(sizeof(*p) + sizeof(p) * 2);
  p->n_type = type;
  p->n_next = NULL;
  p->n_arg[0] = (NODE *) arg0;
  p->n_arg[1] = (NODE *) arg1;
  p->n_arg[2] = (NODE *) arg2;
  return p;
}

NODE *
node4(type, arg0, arg1, arg2, arg3) NODE *arg0, *arg1, *arg2, *arg3;
{
  NODE *p;
  char *emalloc();

  p = (NODE *) emalloc(sizeof(*p) + sizeof(p) * 3);
  p->n_type = type;
  p->n_next = NULL;
  p->n_arg[0] = (NODE *) arg0;
  p->n_arg[1] = (NODE *) arg1;
  p->n_arg[2] = (NODE *) arg2;
  p->n_arg[3] = (NODE *) arg3;
  return p;
}

CELL *
mkcell(type, sval, fval) char *sval; double fval;
{
  CELL *p;
  char *emalloc(), *strsave();

  p = (CELL *) emalloc(sizeof(*p));
  p->c_type = type;
  if (sval == NULL)
	p->c_sval = NULL;
  else
	p->c_sval = strsave(sval);
  p->c_fval = fval;
  return p;
}

#ifdef TMPCELL
#define MAXTMP	25

CELL tmpcell[MAXTMP];
#endif

CELL *
mktmp(type, sval, fval) char *sval; double fval;
{
  register int i;
  char *strsave();

#ifdef TMPCELL
  for (i = 0; i < MAXTMP; i++)
	if (tmpcell[i].c_type == 0) {
		tmpcell[i].c_type = type | TMP;
		tmpcell[i].c_sval = strsave(sval);
		tmpcell[i].c_fval = fval;
		return &tmpcell[i];
	}
  error("formula too complex", (char *) 0);
#else
  return mkcell(type | TMP, sval, fval);
#endif
}

c_free(p) CELL *p;
{
  if ((p != NULL) && (p->c_type & TMP)) {
#ifdef TMPCELL
	p->c_type = 0;
	sfree(p->c_sval);
	p->c_sval = (char *)NULL;
	p->c_fval = 0.0;
#else
	if (p->c_sval != NULL) {
		Free(p->c_sval);
		p->c_sval = NULL;
	}
	p->c_type = 0;
	Free(p);
	p = NULL;
#endif
  }
}
