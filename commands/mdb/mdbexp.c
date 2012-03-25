/*
 * mdbexp.c - MINIX expresion parser
 *
 * Written by Bruce D. Szablak
 *
 * This free software is provided for non-commerical use. No warrantee
 * of fitness for any use is implied. You get what you pay for. Anyone
 * may make modifications and distribute them, but please keep this header
 * in the distribution.
 */

#include "mdb.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto.h"

static long value(char *s , char **s_p , int *seg_p );
static long lookup(char *s , char **s_p , int *seg_p );

#define idchar(c) (isalpha(c) || isdigit(c) || (c) == '_')

/* 
 * Get an expression for mdb
 */
char *getexp(buf, exp_p, seg_p)
char *buf;
int *seg_p;
long *exp_p;
{
  long v = 0L;

  buf = skip(buf);
  if ((isalpha(*buf) && (isspace(buf[1]) || buf[1] == ';')) 
      || *buf == '\n'
      || *buf == ';'
      || *buf == '/'
      || *buf == '!'
      || *buf == '?'
      || *buf == '@'
      || *buf == '#') {
	*exp_p = 0L;
	return buf;
  }
  v = value(buf, &buf, seg_p);
  buf = skip(buf);
  if (*buf == '+')
	v += value(skip(buf + 1), &buf, seg_p);
  else if (*buf == '-')
	v -= value(skip(buf + 1), &buf, seg_p);
  *exp_p = v;
  return skip(buf);
}

/* 
 * Get value 
 *
 * 	\c 	escaped characters
 * 	digits	number
 * 	$xx	registers 
 *	\n	0L
 *	then calls lookup for symbols
 */
static long value(s, s_p, seg_p)
char *s, **s_p;
int *seg_p;
{
  long k;

  if (*s == '\'') {		/* handle character constants here */
	*s_p = s + 2;
	return s[1];
  }
  if (*s == '-' || isdigit(*s))
	return strtol(s, s_p, 0);
  if (*s == '$') {
	k = reg_addr(s + 1);
	*s_p = s + 3;
	return get_reg(curpid, k);
	k = reg_addr(s + 1);
	*s_p = s + 3;
	return get_reg(curpid, k);
  }
  if (*s == '\n') {
	*s_p = s + 1;
	return 0L;
  }
  return lookup(s, s_p, seg_p);
}

/* 
 * Lookup symbol - return value
 * Handle special cases: _start T: D: S: 
 * then call symbolvalue()
 */
static long lookup(s, s_p, seg_p)
char *s, **s_p;
int *seg_p;
{
  long value;
  char c;
  int l;

  for (l = 1; idchar(s[l]); ++l) {}
  c = s[l];
  s[l] = 0;

  if (strcmp("_start", s) == 0) {
	*seg_p = T;
	if (c == ':') c = '+';
	*(*s_p = s + 6) = c;
	return st_addr;
  }
  if (strcmp("T", s) == 0) {
	*seg_p = T;
	if (c == ':') c = '+';
	*(*s_p = s + 1) = c;
	return st_addr;
  }
  if (strcmp("D", s) == 0) {
	*seg_p = D;
	if (c == ':') c = '+';
	*(*s_p = s + 1) = c;
	return sd_addr;
  }
  if (strcmp("S", s) == 0) {
	*seg_p = S;
	if (c == ':') c = '+';
	*(*s_p = s + 1) = c;
	return sk_addr;
  }

  if ((value = symbolvalue(s, TRUE)) != 0L) {
	*seg_p = T;
	*(*s_p = s + l) = c;
	return value;
  }

  if ((value = symbolvalue(s, FALSE)) != 0L) {
	*seg_p = D;
	*(*s_p = s + l) = c;
	return value;
  }

  Printf("%s: ", s);
  mdb_error("symbol not found\n");
}

/* Skip spaces */
char *skip(s)
register char *s;
{
  while (isspace(*s)) ++s;
  return *s ? s : s - 1;
}

