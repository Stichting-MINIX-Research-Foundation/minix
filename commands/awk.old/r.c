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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef DOS
#include <process.h>
#endif
#include "awk.h"
#include "regexp.h"

#define MAXFLD	100

extern char **FS, **RS, **OFS, **ORS, **FILENAME;
extern double *NF, *NR;
extern double *FNR;
extern double *ARGC;
extern SYMBOL *argtab[];
extern CELL *getvar();

char *strsave(), *strcpy(), *getsval(), *jStrchr(), *strchr();
double getfval(), atof();
CELL *mkcell(), *mktmp(), *execute(), *patexec();
FILE *efopen();

extern CELL truecell, falsecell;

extern int pateval;

int infileno = 1;
FILE *ifp;
char record[BUFSIZ];
CELL *field[MAXFLD];

char *fs_str;
regexp *fs_pat;

CELL *
Getline(p) NODE *p;
{
  CELL *u;
  char *fnam, *s, str[BUFSIZ];
  int i;
  FILE *fp, *getfp();

  if ((int) p->n_arg[0])	/* read into var */
	s = str;
  else
	s = NULL;
  if ((int) p->n_arg[1]) {	/* file name */
	u = execute(p->n_arg[1]);
	fnam = getsval(u);
	fp = getfp(fnam, (int) p->n_arg[2]);
	c_free(u);
	i = get1rec(s, fp);
  }
  else
	i = Getrec(s);
  if (s == str) {
	u = execute(p->n_arg[0]);
	setsval(u, str);
  }
  return mktmp(NUM, NULL, (double) i);
}

static
get1rec(buf, fp) char *buf; FILE *fp;
{
  register int c;
  register char rs, *s;
  int mflg;

  if (buf == NULL)
	buf = record;
  if ((rs = **RS) == '\0') {	/* multi line record */
	mflg = 1;
	rs = '\n';
  }
  else
	mflg = 0;

  if (feof(fp) || (c = getc(fp)) == EOF)
	return 0;
  for (s = buf; ; ) {
	for ( ; c != rs && c != EOF; c = getc(fp)) {
		if (isKanji(c)) {
			*s++ = c; c = getc(fp);
		}
		*s++ = c;
	}
	if (mflg) {
		if ((c = getc(fp)) == '\n' || c == EOF)
			break;
		*s++ = '\n';
	}
	else
		break;
  }
  *s = '\0';
#if 1
  if (buf == record) {
#else
  if (buf == record && c != EOF) {
#endif
	mkfld(record, *FS, field);
	(*NR)++;
	(*FNR)++;
  }
  return s > buf || c != EOF ? 1 : 0;
}

Getrec(s) char *s;
{
  CELL *u;
  char *file, str[8];

  while (ifp == stdin || infileno < (int)*ARGC) {
	if (ifp == NULL) {
		*FNR = 0.0;
		if (infileno == (int)*ARGC)
			break;
		sprintf(str, "%d", infileno);
		u = getvar(str, argtab);
		file = getsval(u);
		if (strchr(file, '=') != NULL) {
			setvar(file);
			infileno++;
			continue;
		}
		else if (strcmp(file, "") == 0) {
/*
if (infileno == (int)*ARGC - 1)
			ifp = stdin;
*/
			infileno++;
			continue;
		}
		else {
			if (strcmp(file, "-") == 0)
				ifp = stdin;
			else
				ifp = efopen(file, "r");
			*FILENAME = file;
		}
	}
	if (get1rec(s, ifp))
		return 1;
	else {
		if (ifp != stdin)
			fclose(ifp);
		ifp = NULL;
		infileno++;
	}
  }
  ifp = stdin;	/* for further "getline" */
  *FILENAME = "-";
  return 0;	/* EOF */
}

mkfld(rec, sep, fld) char *rec, *sep; CELL *fld[];
{
  char *s, *t;
  char str[BUFSIZ];
  int i, j, n;
  int skip = 0;

  if (strlen(sep) > 1)
	return r_mkfld(rec, sep, fld);

  if (*sep == ' ' || *sep == '\0') {
	sep = " \t\n"; skip++;
  }
  for (i = 1, n = (int) *NF; i <= n; i++) {
	sfree(fld[i]->c_sval);
	sfree(fld[i]);
	fld[i] = NULL;
  }
  for (i = 0, s = rec; ; ) {
	t = str;
	if (skip) {
		while (*s && strchr(" \t\n", *s))
			s++;
		if (*s == '\0')
			break;
	}
	while (*s && !jStrchr(sep, *s)) {
		if (isKanji(*s))
			*t++ = *s++;
		*t++ = *s++;
	}
	*t = '\0';
	if (isnum(str))
		fld[++i] =  mkcell(FLD|STR|NUM, str, atof(str));
	else
		fld[++i] =  mkcell(FLD|STR, str, 0.0);
	if (*s)
		s++;
	else
		break;
  }
  *NF = (double) i;
  return i;
}

static
r_mkfld(rec, sep, fld) char *rec, *sep; CELL *fld[];
{
  char *s, *t;
  char str[BUFSIZ];
  int i, n;
  regexp *mkpat();
  extern int r_start, r_length;

  if (strcmp(*FS, fs_str) != 0) {
	sfree(fs_str); sfree(fs_pat);
	fs_str = strsave(*FS);
	fs_pat = mkpat(fs_str);
  }
  for (i = 1, n = (int) *NF; i <= n; i++) {
	sfree(fld[i]->c_sval);
	sfree(fld[i]);
	fld[i] = NULL;
  }
  for (i = 0, s = rec, t = str; *s; ) {
	if (match(fs_pat, s)) {
		for (n = r_start; --n > 0; )
			*t++ = *s++;
	}
	else {
		while (*s)
			*t++ = *s++;
	}
	*t = '\0';
	t = str;
	fld[++i] = mkcell(FLD|STR, str, 0.0);
	if (*s)
		s += r_length;
  }
  *NF = (double) i;
  return i;
}

mkrec(u) CELL *u;
{
  register char *s, *t;
  register int i, j;

  for (j = (int)*NF, i = 1; i <= j; i++)
	if (field[i] == u)
		break;
  if (i > j) {
	for ( ; i < MAXFLD; i++)
		if (field[i] == u)
			break;
	if (i == MAXFLD)
		error("too many field (%d)", i);
	*NF = (double)i;
  }
  for (t = record, i = 1, j = (int) *NF; i <= j; i++) {
	if (i > 1)
		*t++ = **OFS;
	for (s = getsval(field[i]); *s; )
		*t++ = *s++;
  }
  *t++ = '\0';
}

CELL *
Field(p) NODE *p;
{
  CELL *u;
  int i, j;

  u = execute(p->n_arg[0]);
  i = (int) getfval(u);
  c_free(u);
  j = (int)*NF;
  if (i > j)
	for (++j; j <= i; j++) {
		if (field[j] == NULL)
			field[j] = mkcell(FLD|STR, "", 0.0);
	}
  return field[i];
}

CELL *
P1stat(p) NODE *p;
{
  CELL *u;
  double x;

  pateval++;
  u = execute(p->n_arg[0]);
  pateval = 0;
  x = getfval(u);
  c_free(u);
  if (x != 0.0)
	u = execute(p->n_arg[1]);
  else
	u = &truecell;
  return u;
}

CELL *
P2stat(p) NODE *p;
{
  static stat = 0;
  CELL *u, *v;
  double x;

  switch (stat) {
  case 0:
	pateval++;
	u = execute(p->n_arg[0]);
	pateval = 0;
	x = getfval(u);
	c_free(u);
	if (x == 0.0) {
		u = &truecell; break;
	}
	else
		stat++;
	/* fall through */
  case 1:
	u = execute(p->n_arg[2]);
	c_free(u);
	pateval++;
	u = execute(p->n_arg[1]);
	pateval = 0;
	x = getfval(u);
	if (x != 0.0)
		stat = 0;
	break;
  default:
	u = &truecell;
	break;
  }
  return u;
}

CELL *
Print0()
{
/*
  int i, j;
  char *s, str[BUFSIZ];

  for (*str = '\0', i = 1, j = (int) *NF; i <= j; i++) {
	if (i > 1)
		strcat(str, *OFS);
	s = getsval(field[i]);
	strcat(str, s);
  }
  strcat(str, *ORS);
  fputs(str, stdout);
*/
  fprintf(stdout, "%s%s", record, *ORS);
  return &truecell;
}

char *
format(t, p) char *t; NODE *p;
{
  CELL *u, *v;
  char *r, *s, *s0, fmt[BUFSIZ];
  double x;
  int i;

  u = execute(p->n_arg[2]);
  s = s0 = getsval(u);
/*
printf("fmt(%s)\n", s);
*/
  for (i = 3; *s; s++) {
	if (isKanji(*s)) {
		*t++ = *s++; *t++ = *s; continue;
	}
	if (*s != '%') {
		*t++ = *s; continue;
	}
	else if (*(s + 1) == '%') {
		*t++ = *s++; continue;
	}
	for (r = fmt, *r++ = *s++; *r++ = *s; s++) {
		if (strchr("%cdefgosux", *s))
			break;
	}
	*r = '\0';
	if (p->n_arg[i] == NULL)
		error("not enough args in printf(%s)", s0);
	v = execute(p->n_arg[i++]);
	if (*s == 's')
		r = getsval(v);
	else
		x = getfval(v);
/*
printf("val(%d)(%s)\n", v->c_type, v->c_sval);
*/
	switch (*s) {
	case 'c':
		sprintf(t, fmt, (int) x);
		break;
	case 'd':
		if (*(s - 1) != 'l') {
			*--r = 'l'; *++r = 'd'; *++r = '\0';
		}
		sprintf(t, fmt, (long) x);
		break;
	case 'e': case 'f': case 'g':
		sprintf(t, fmt, x);
		break;
	case 'o': case 'u': case 'x':
		if (*(s - 1) == 'l')
			sprintf(t, fmt, (long) x);
		else
			sprintf(t, fmt, (int) x);
		break;
	case 's':
		/*r = getsval(v);*/
		sprintf(t, fmt, r);
		break;
	default:
		strcpy(t, fmt);
		break;
	}
	c_free(v);
	t += strlen(t);
  }
  c_free(u);
  *t = '\0';
}

#define MAXFILE	10
struct {
  char *f_name;	/* file name */
  FILE *f_fp;
  int f_type;
} filetab[MAXFILE];

FILE *
getfp(file, type) char *file;
{
  register int i;
  register char *name, *mode;
  char *awktmp();
  FILE *fp, *efopen(), *epopen();

  for (i = 0; i < MAXFILE; i++)
	if (filetab[i].f_name && strcmp(filetab[i].f_name, file) == 0)
		return filetab[i].f_fp;
  for (i = 0; i < MAXFILE; i++)
	if (!filetab[i].f_fp)
		break;
  if (i == MAXFILE)
	error("too many files to open");
  name = file;
  switch (type) {
  case R_OUT:	mode = "w"; break;
  case R_APD:	mode = "a"; break;
  case R_POUT:
#ifdef DOS
	name = awktmp(i); mode = "w";	/* MS-DOS */
#else
	fp = epopen(file, "w");
	goto g1;
#endif
	break;
  case R_IN:	mode = "r"; break;
  case R_PIN:
#ifdef DOS
	{
		int savefd, fd, result;

		name = awktmp(i);
		if ((fd = open(name,
		O_WRONLY|O_TEXT|O_CREAT|O_TRUNC,S_IREAD|S_IWRITE)) == -1)
			error("can't open %s", name);
		savefd = dup(1); dup2(fd, 1); close(fd);
		if ((result =
			system(file)) == -1)
			error("can't exec %s", file);
		dup2(savefd, 1); close(savefd); close(fd);
		mode = "r";
	}
#else
	fp = epopen(file,"r");
	goto g1;
#endif
	break;
  }
  fp = efopen(name, mode);
g1:
  filetab[i].f_name = strsave(file);
  filetab[i].f_type = type;
  filetab[i].f_fp = fp;
  return fp;
}

closeall()
{
  register int i;

  for (i = 0; i < MAXFILE; i++)
	close1(i);
}

CELL *
Close(s) char *s;
{
  register int i;

  for (i = 0; i < MAXFILE; i++)
	if (strcmp(s, filetab[i].f_name) == 0) {
		close1(i);
		break;
	}
  i = (i == MAXFILE) ? 0 : 1;
  return mktmp(NUM, NULL, (double) i);
}

static
close1(i)
{
  int fd, result, savefd;
  char *awktmp();

  if (filetab[i].f_fp == NULL)
	return;
  switch (filetab[i].f_type) {
  case R_PIN:
#ifdef DOS
	fclose(filetab[i].f_fp);
	unlink(awktmp(i));
#else
	pclose(filetab[i].f_fp);
#endif
	break;
  case R_IN: case R_OUT: case R_APD:
	fclose(filetab[i].f_fp);
	break;
  case R_POUT:
#ifdef DOS
	fclose(filetab[i].f_fp);
	if ((fd = open(awktmp(i), O_RDONLY)) == NULL)
		error("can't open %s", awktmp(i));
	savefd = dup(0);
	dup2(fd, 0);
	close(fd);
	if ((result =
		system(filetab[i].f_name)) == -1)
/*
	spawnl(P_WAIT, "/usr/bin/sh", "sh", "-c", filetab[i].f_name, (char *) 0)) == -1)
		fprintf(stderr, "can't spawn /bin/sh\n");
*/
		error("can't exec %s", filetab[i].f_name);
	dup2(savefd, 0);
	close(savefd);
	unlink(awktmp(i));
#else
	pclose(filetab[i].f_fp);
#endif
	break;
  }
  sfree(filetab[i].f_name);
  filetab[i].f_type = 0;
  filetab[i].f_name = NULL;
  filetab[i].f_fp = NULL;
}

#ifndef DOS
FILE *
epopen(file, mod) char *file, *mod;
{
  FILE *fp, *popen();

  if ((fp = popen(file, mod)) == NULL)
	error("can't poen %s", file);
  return fp;
}
#endif

static char *
awktmp(i)
{
  static char str[16];

  sprintf(str, "awk000%02d.tmp", i);
  return str;
}

Index(s, t) char *s, *t;
{
  register char *u, *v;
  register int i;

  for (i = 1; *s; s++, i++) {
	for (u = s, v = t; *v; u++, v++) {
		if (isKanji(*v)) {
			if (*u != *v)
				break;
			u++; v++;
		}
		if (*u != *v)
			break;
	}
	if (*v == '\0')
		return i;
	if (isKanji(*s))
		s++;
  }
  return 0;
}
