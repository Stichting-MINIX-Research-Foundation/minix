/* find - look for files satisfying a predicate       Author: E. Baalbergen */

/* Original author: Erik Baalbergen; POSIX compliant version: Bert Laverman */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <pwd.h>
#include <grp.h>
#include <dirent.h>
#include <limits.h>
#include <stdio.h>

/*######################## DEFINITIONS ##############################*/

#ifdef S_IFLNK
#define LSTAT lstat
#else
#define LSTAT stat
#endif

#define SHELL "/bin/sh"
#define MAXARG          256	/* maximum length for an argv for -exec  */
#define BSIZE           512	/* POSIX wants 512 byte blocks           */
#define SECS_PER_DAY    (24L*60L*60L)	/* check your planet             */

#define OP_NAME          1	/* match name                            */
#define OP_PERM          2	/* check file permission bits            */
#define OP_TYPE          3	/* check file type bits                  */
#define OP_LINKS         4	/* check link count                      */
#define OP_USER          5	/* check owner                           */
#define OP_GROUP         6	/* check group ownership                 */
#define OP_SIZE          7	/* check size, blocks or bytes           */
#define OP_SIZEC         8	/* this is a fake for -size with 'c'     */
#define OP_INUM          9	/* compare inode number                  */
#define OP_ATIME        10	/* check last access time                */
#define OP_CTIME        11	/* check creation time                   */
#define OP_MTIME        12	/* check last modification time          */
#define OP_EXEC         13	/* execute command                       */
#define OP_OK           14	/* execute with confirmation             */
#define OP_PRINT        15	/* print name                            */
#define OP_PRINT0       16	/* print name null terminated            */
#define OP_NEWER        17	/* compare modification times            */
#define OP_AND          18	/* logical and (short circuit)           */
#define OP_OR           19	/* logical or (short circuit)            */
#define OP_XDEV         20	/* do not cross file-system boundaries   */
#define OP_DEPTH        21	/* descend directory before testing      */
#define OP_PRUNE        22	/* don't descend into current directory  */
#define OP_NOUSER       23	/* check validity of user id             */
#define OP_NOGROUP      24	/* check validity of group id            */
#define LPAR            25	/* left parenthesis                      */
#define RPAR            26	/* right parenthesis                     */
#define NOT             27	/* logical not                           */

/* Some return values: */
#define EOI             -1	/* end of expression                     */
#define NONE             0	/* not a valid predicate                 */

/* For -perm with symbolic modes: */
#define ISWHO(c)        ((c == 'u') || (c == 'g') || (c == 'o') || (c == 'a'))
#define ISOPER(c)       ((c == '-') || (c == '=') || (c == '+'))
#define ISMODE(c)       ((c == 'r') || (c == 'w') || (c == 'x') || \
			 (c == 's') || (c == 't'))
#define MUSER           1
#define MGROUP          2
#define MOTHERS         4


struct exec {
  int e_cnt;
  char *e_vec[MAXARG];
};

struct node {
  int n_type;			/* any OP_ or NOT */
  union {
	char *n_str;
	struct {
		long n_val;
		int n_sign;
	} n_int;
	struct exec *n_exec;
	struct {
		struct node *n_left, *n_right;
	} n_opnd;
  } n_info;
};

struct oper {
  char *op_str;
  int op_val;
} ops[] = {

  {
	"name", OP_NAME
  },
  {
	"perm", OP_PERM
  },
  {
	"type", OP_TYPE
  },
  {
	"links", OP_LINKS
  },
  {
	"user", OP_USER
  },
  {
	"group", OP_GROUP
  },
  {
	"size", OP_SIZE
  },
  {
	"inum", OP_INUM
  },
  {
	"atime", OP_ATIME
  },
  {
	"ctime", OP_CTIME
  },
  {
	"mtime", OP_MTIME
  },
  {
	"exec", OP_EXEC
  },
  {
	"ok", OP_OK
  },
  {
	"print", OP_PRINT
  },
  {
	"print0", OP_PRINT0
  },
  {
	"newer", OP_NEWER
  },
  {
	"a", OP_AND
  },
  {
	"o", OP_OR
  },
  {
	"xdev", OP_XDEV
  },
  {
	"depth", OP_DEPTH
  },
  {
	"prune", OP_PRUNE
  },
  {
	"nouser", OP_NOUSER
  },
  {
	"nogroup", OP_NOGROUP
  },
  {
	0, 0
  }
};


char **ipp;			/* pointer to next argument during parsing       */
char *prog;			/* program name (== argv [0])                    */
char *epath;			/* value of PATH environment string              */
long current_time;		/* for computing age                             */
int tty;			/* fd for /dev/tty when using -ok                */
int xdev_flag = 0;		/* cross device boundaries?                      */
int devnr;			/* device nr of first inode                      */
int depth_flag = 0;		/* descend before check?                         */
int prune_here;			/* This is Baaaad! Don't ever do this again!     */
int um;				/* current umask()                               */
int needprint = 1;		/* implicit -print needed?                       */


/* The prototypes: */
_PROTOTYPE(int main, (int argc, char **argv));
_PROTOTYPE(char *Malloc, (int n));
_PROTOTYPE(char *Salloc, (char *s));
_PROTOTYPE(void find, (char *path, struct node * pred, char *last));
_PROTOTYPE(int check, (char *path, struct stat * st, struct node * n, char *last));
_PROTOTYPE(int ichk, (long val, struct node * n));
_PROTOTYPE(int lex, (char *str));
_PROTOTYPE(struct node * newnode, (int t));
_PROTOTYPE(int isnumber, (char *str, int base, int sign));
_PROTOTYPE(void number, (char *str, int base, long *pl, int *ps));
_PROTOTYPE(void fmode, (char *str, long *pl, int *ps));
_PROTOTYPE(struct node * expr, (int t));
_PROTOTYPE(struct node * primary, (int t));
_PROTOTYPE(struct node * secondary, (int t));
_PROTOTYPE(void checkarg, (char *arg));
_PROTOTYPE(struct node * simple, (int t));
_PROTOTYPE(void nonfatal, (char *s1, char *s2));
_PROTOTYPE(void fatal, (char *s1, char *s2));
_PROTOTYPE(int smatch, (char *s, char *t));
_PROTOTYPE(char *find_bin, (char *s));
_PROTOTYPE(int execute, (int op, struct exec * e, char *path));
_PROTOTYPE(void domode, (int op, int *mode, int bits));


/* Malloc: a certified malloc */
char *Malloc(n)
int n;
{
  char *m;

  if ((m = (char *) malloc(n)) == (char *) NULL) fatal("out of memory", "");
  return m;
}

/* Salloc: allocate space for a string */
char *Salloc(s)
char *s;
{
  return strcpy(Malloc(strlen(s) + 1), s);
}


/* Main: the main body */
int main(argc, argv)
int argc;
char *argv[];
{
  char **pathlist, *path, *last;
  int pathcnt = 0, i;
  struct node *pred;

  prog = *argv++;		/* set program name (for diagnostics)    */
  if ((epath = getenv("PATH")) == (char *) NULL)
	fatal("Can't get path from environment", "");
  (void) umask(um = umask(0));	/* non-destructive get-umask :-)         */
  time(&current_time);		/* get current time                      */

  pathlist= argv;
  while (--argc > 0 && lex(*argv) == NONE) {	/* find paths            */
	pathcnt++;
	argv++;
  }
  if (pathcnt == 0)		/* there must be at least one path       */
	fatal("Usage: path-list [predicate-list]", "");

  ipp = argv;			/* prepare for parsing                   */
  if (argc != 0) {		/* If there is anything to parse,        */
	pred = expr(lex(*ipp));	/* then do so                            */
	if (lex(*++ipp) != EOI)	/* Make sure there's nothing left        */
		fatal("syntax error: garbage at end of predicate", "");
  } else			/* No predicate list                     */
	pred = (struct node *) NULL;

  for (i = 0; i < pathcnt; i++) {
	if (xdev_flag) xdev_flag = 2;
	path = pathlist[i];
	if ((last = strrchr(path, '/')) == NULL) last = path; else last++;
	find(path, pred, last);
  }
  return 0;
}

void find(path, pred, last)
char *path, *last;
struct node *pred;
{
  char spath[PATH_MAX];
  register char *send = spath, *p;
  struct stat st;
  DIR *dp;
  struct dirent *de;

  if (path[1] == '\0' && *path == '/') {
	*send++ = '/';
	*send = '\0';
  } else
	while (*send++ = *path++) {
	}

  if (LSTAT(spath, &st) == -1)
	nonfatal("can't get status of ", spath);
  else {
	switch (xdev_flag) {
	  case 0:
		break;
	  case 1:
		if (st.st_dev != devnr) return;
		break;
	  case 2:		/* set current device number */
		xdev_flag = 1;
		devnr = st.st_dev;
		break;
	}

	prune_here = 0;
	if (!depth_flag && check(spath, &st, pred, last) && needprint)
		printf("%s\n", spath);
	if (!prune_here && (st.st_mode & S_IFMT) == S_IFDIR) {
		if ((dp = opendir(spath)) == NULL) {
			nonfatal("can't read directory ", spath);
			perror( "Error" );
			return;
		}
		send[-1] = '/';
		while ((de = readdir(dp)) != NULL) {
			p = de->d_name;
			if ((de->d_name[0] != '.') || ((de->d_name[1])
					  && ((de->d_name[1] != '.')
					      || (de->d_name[2])))) {
				strcpy(send, de->d_name);
				find(spath, pred, send);
			}
		}
		closedir(dp);
	}
	if (depth_flag) {
		send[-1] = '\0';
		if (check(spath, &st, pred, last) && needprint)
			printf("%s\n", spath);
	}
  }
}

int check(path, st, n, last)
char *path, *last;
register struct stat *st;
register struct node *n;
{
  if (n == (struct node *) NULL) return 1;
  switch (n->n_type) {
    case OP_AND:
	return check(path, st, n->n_info.n_opnd.n_left, last) &&
		check(path, st, n->n_info.n_opnd.n_right, last);
    case OP_OR:
	return check(path, st, n->n_info.n_opnd.n_left, last) ||
		check(path, st, n->n_info.n_opnd.n_right, last);
    case NOT:
	return !check(path, st, n->n_info.n_opnd.n_left, last);
    case OP_NAME:
	return smatch(last, n->n_info.n_str);
    case OP_PERM:
	if (n->n_info.n_int.n_sign < 0)
		return(st->st_mode & (int) n->n_info.n_int.n_val) ==
			(int) n->n_info.n_int.n_val;
	return(st->st_mode & 07777) == (int) n->n_info.n_int.n_val;
    case OP_NEWER:
	return st->st_mtime > n->n_info.n_int.n_val;
    case OP_TYPE:
	return(st->st_mode & S_IFMT) == (mode_t) n->n_info.n_int.n_val;
    case OP_LINKS:
	return ichk((long) (st->st_nlink), n);
    case OP_USER:
	return st->st_uid == n->n_info.n_int.n_val;
    case OP_GROUP:
	return st->st_gid == n->n_info.n_int.n_val;
    case OP_SIZE:
	return ichk((st->st_size == 0) ? 0L :
		    (long) ((st->st_size - 1) / BSIZE + 1), n);
    case OP_SIZEC:
	return ichk((long) st->st_size, n);
    case OP_INUM:
	return ichk((long) (st->st_ino), n);
    case OP_ATIME:
	return ichk(st->st_atime, n);
    case OP_CTIME:
	return ichk(st->st_ctime, n);
    case OP_MTIME:
	return ichk(st->st_mtime, n);
    case OP_EXEC:
    case OP_OK:
	return execute(n->n_type, n->n_info.n_exec, path);
    case OP_PRINT:
	printf("%s\n", path);
	return 1;
    case OP_PRINT0:
	printf("%s", path); putchar(0);
	return 1;
    case OP_XDEV:
    case OP_DEPTH:
	return 1;
    case OP_PRUNE:
	prune_here = 1;
	return 1;
    case OP_NOUSER:
	return(getpwuid(st->st_uid) == (struct passwd *) NULL);
    case OP_NOGROUP:
	return(getgrgid(st->st_gid) == (struct group *) NULL);
  }
  fatal("ILLEGAL NODE", "");
  return 0;			/* Never reached */
}

int ichk(val, n)
long val;
struct node *n;
{
  switch (n->n_info.n_int.n_sign) {
    case 0:
	return val == n->n_info.n_int.n_val;
    case 1:
	return val > n->n_info.n_int.n_val;
    case -1:	return val < n->n_info.n_int.n_val;
}
  fatal("internal: bad n_sign", "");
  return 0;			/* Never reached */
}

int lex(str)
char *str;
{
  if (str == (char *) NULL) return EOI;
  if (*str == '-') {
	register struct oper *op;

	str++;
	for (op = ops; op->op_str; op++)
		if (strcmp(str, op->op_str) == 0) break;
	return op->op_val;
  }
  if (str[1] == 0) {
	switch (*str) {
	  case '(':
		return LPAR;
	  case ')':
		return RPAR;
	  case '!':	return NOT;
	}
  }
  return NONE;
}

struct node *
 newnode(t)
int t;
{
  struct node *n = (struct node *) Malloc(sizeof(struct node));

  n->n_type = t;
  return n;
}

/*########################### PARSER ###################################*/
/* Grammar:
 * expr        : primary | primary OR expr;
 * primary     : secondary | secondary AND primary | secondary primary;
 * secondary   : NOT secondary | LPAR expr RPAR | simple;
 * simple      : -OP args...
 */

/* Isnumber checks correct number syntax. A sign is allowed, but the '+'
 * only if the number is to be in decimal.
 */
int isnumber(str, base, sign)
register char *str;
int base;
int sign;
{
  if (sign && ((*str == '-') || ((base == 8) && (*str == '+')))) str++;
  while ((*str >= '0') && (*str < ('0' + base))) str++;
  return(*str == '\0' ? 1 : 0);
}

/* Convert a string to an integer, storing sign info in *ps. */
void number(str, base, pl, ps)
char *str;
int base;
long *pl;
int *ps;
{
  int up = '0' + base - 1;
  long val = 0;

  *ps = ((*str == '-' || *str == '+') ? ((*str++ == '-') ? -1 : 1) : 0);
  while (*str >= '0' && *str <= up) val = base * val + *str++ - '0';
  if (*str) fatal("syntax error: illegal numeric value", "");
  *pl = val;
}


void domode(op, mode, bits)
int op;
int *mode;
int bits;
{
  switch (op) {
    case '-':
	*mode &= ~bits;
	break;			/* clear bits */
    case '=':
	*mode |= bits;
	break;			/* set bits */
    case '+':
	*mode |= (bits & ~um);	/* set, but take umask in account */
  }
}

void fmode(str, pl, ps)
char *str;
long *pl;
int *ps;
{
  int m = 0, w, op;
  char *p = str;

  if (*p == '-') {
	*ps = -1;
	p++;
  } else
	*ps = 0;

  while (*p) {
	w = 0;
	if (ISOPER(*p))
		w = MUSER | MGROUP | MOTHERS;
	else if (!ISWHO(*p))
		fatal("u, g, o, or a expected: ", p);
	else {
		while (ISWHO(*p)) {
			switch (*p) {
			  case 'u':
				w |= MUSER;
				break;
			  case 'g':
				w |= MGROUP;
				break;
			  case 'o':
				w |= MOTHERS;
				break;
			  case 'a':
				w = MUSER | MGROUP | MOTHERS;
			}
			p++;
		}
		if (!ISOPER(*p)) fatal("-, + or = expected: ", p);
	}
	op = *p++;
	while (ISMODE(*p)) {
		switch (*p) {
		  case 'r':
			if (w & MUSER) domode(op, &m, S_IRUSR);
			if (w & MGROUP) domode(op, &m, S_IRGRP);
			if (w & MOTHERS) domode(op, &m, S_IROTH);
			break;
		  case 'w':
			if (w & MUSER) domode(op, &m, S_IWUSR);
			if (w & MGROUP) domode(op, &m, S_IWGRP);
			if (w & MOTHERS) domode(op, &m, S_IWOTH);
			break;
		  case 'x':
			if (w & MUSER) domode(op, &m, S_IXUSR);
			if (w & MGROUP) domode(op, &m, S_IXGRP);
			if (w & MOTHERS) domode(op, &m, S_IXOTH);
			break;
		  case 's':
			if (w & MUSER) domode(op, &m, S_ISUID);
			if (w & MGROUP) domode(op, &m, S_ISGID);
			break;
		  case 't':
			domode(op, &m, S_ISVTX);
		}
		p++;
	}
	if (*p) {
		if (*p == ',')
			p++;
		else
			fatal("garbage at end of mode string: ", p);
	}
  }
  *pl = m;
}

struct node *
 expr(t)
int t;
{
  struct node *nd, *p, *nd2;

  nd = primary(t);
  if ((t = lex(*++ipp)) == OP_OR) {
	nd2 = expr(lex(*++ipp));
	p = newnode(OP_OR);
	p->n_info.n_opnd.n_left = nd;
	p->n_info.n_opnd.n_right = nd2;
	return p;
  }
  ipp--;
  return nd;
}

struct node *
 primary(t)
int t;
{
  struct node *nd, *p, *nd2;

  nd = secondary(t);
  if ((t = lex(*++ipp)) != OP_AND) {
	ipp--;
	if (t == EOI || t == RPAR || t == OP_OR) return nd;
  }
  nd2 = primary(lex(*++ipp));
  p = newnode(OP_AND);
  p->n_info.n_opnd.n_left = nd;
  p->n_info.n_opnd.n_right = nd2;
  return p;
}

struct node *
 secondary(t)
int t;
{
  struct node *n, *p;

  if (t == LPAR) {
	n = expr(lex(*++ipp));
	if (lex(*++ipp) != RPAR) fatal("syntax error, ) expected", "");
	return n;
  }
  if (t == NOT) {
	n = secondary(lex(*++ipp));
	p = newnode(NOT);
	p->n_info.n_opnd.n_left = n;
	return p;
  }
  return simple(t);
}

void checkarg(arg)
char *arg;
{
  if (arg == 0) fatal("syntax error, argument expected", "");
}

struct node *
 simple(t)
int t;
{
  struct node *p = newnode(t);
  struct exec *e;
  struct stat est;
  struct passwd *pw;
  struct group *gr;
  long l;
  int i;

  switch (t) {
    case OP_TYPE:
	checkarg(*++ipp);
	switch (**ipp) {
	  case 'b':
		p->n_info.n_int.n_val = S_IFBLK;
		break;
	  case 'c':
		p->n_info.n_int.n_val = S_IFCHR;
		break;
	  case 'd':
		p->n_info.n_int.n_val = S_IFDIR;
		break;
	  case 'f':
		p->n_info.n_int.n_val = S_IFREG;
		break;
	  case 'l':
#ifdef S_IFLNK
		p->n_info.n_int.n_val = S_IFLNK;
#else
		p->n_info.n_int.n_val = ~0;	/* Always unequal. */
#endif
		break;
	  default:
		fatal("-type needs b, c, d, f or l", "");
	}
	break;
    case OP_USER:
	checkarg(*++ipp);
	if (((pw = getpwnam(*ipp)) == NULL)
	    && isnumber(*ipp, 10, 0))
		number(*ipp, 10, &(p->n_info.n_int.n_val),
		       &(p->n_info.n_int.n_sign));
	else {
		if (pw == NULL)
			fatal("unknown user: ", *ipp);
		p->n_info.n_int.n_val = pw->pw_uid;
		p->n_info.n_int.n_sign = 0;
	}
	break;
    case OP_GROUP:
	checkarg(*++ipp);
	if (((gr = getgrnam(*ipp)) == NULL)
	    && isnumber(*ipp, 10, 0))
		number(*ipp, 10, &(p->n_info.n_int.n_val),
		       &(p->n_info.n_int.n_sign));
	else {
		if (gr == NULL)
			fatal("unknown group: ", *ipp);
		p->n_info.n_int.n_val = gr->gr_gid;
		p->n_info.n_int.n_sign = 0;
	}
	break;
    case OP_SIZE:
	checkarg(*++ipp);
	i = strlen(*ipp) - 1;
	if ((*ipp)[i] == 'c') {
		p->n_type = OP_SIZEC;	/* Count in bytes i.s.o. blocks */
		(*ipp)[i] = '\0';
	}
	number(*ipp, 10, &(p->n_info.n_int.n_val),
	       &(p->n_info.n_int.n_sign));
	break;
    case OP_LINKS:
    case OP_INUM:
	checkarg(*++ipp);
	number(*ipp, 10, &(p->n_info.n_int.n_val),
	       &(p->n_info.n_int.n_sign));
	break;
    case OP_PERM:
	checkarg(*++ipp);
	if (isnumber(*ipp, 8, 1)) number(*ipp, 8, &(p->n_info.n_int.n_val),
		       &(p->n_info.n_int.n_sign));
	else
		fmode(*ipp, &(p->n_info.n_int.n_val),
		      &(p->n_info.n_int.n_sign));
	break;
    case OP_ATIME:
    case OP_CTIME:
    case OP_MTIME:
	checkarg(*++ipp);
	number(*ipp, 10, &l, &(p->n_info.n_int.n_sign));
	p->n_info.n_int.n_val = current_time - l * SECS_PER_DAY;
	/* More than n days old means less than the absolute time */
	p->n_info.n_int.n_sign *= -1;
	break;
    case OP_EXEC:
    case OP_OK:
	checkarg(*++ipp);
	e = (struct exec *) Malloc(sizeof(struct exec));
	e->e_cnt = 2;
	e->e_vec[0] = SHELL;
	p->n_info.n_exec = e;
	while (*ipp) {
		if (**ipp == ';' && (*ipp)[1] == '\0') {
			e->e_vec[e->e_cnt] = 0;
			break;
		}
		e->e_vec[(e->e_cnt)++] =
			(**ipp == '{' && (*ipp)[1] == '}'
		       && (*ipp)[2] == '\0') ? (char *) (-1) : *ipp;
		ipp++;
	}
	if (*ipp == 0) fatal("-exec/-ok: ; missing", "");
	if ((e->e_vec[1] = find_bin(e->e_vec[2])) == 0)
		fatal("can't find program ", e->e_vec[2]);
	if (t == OP_OK)
		if ((tty = open("/dev/tty", O_RDWR)) < 0)
			fatal("can't open /dev/tty", "");
	break;
    case OP_NEWER:
	checkarg(*++ipp);
	if (LSTAT(*ipp, &est) == -1)
		fatal("-newer: can't get status of ", *ipp);
	p->n_info.n_int.n_val = est.st_mtime;
	break;
    case OP_NAME:
	checkarg(*++ipp);
	p->n_info.n_str = *ipp;
	break;
    case OP_XDEV:	xdev_flag = 1;	break;
    case OP_DEPTH:	depth_flag = 1;	break;
    case OP_PRUNE:
    case OP_PRINT:
    case OP_PRINT0:
    case OP_NOUSER:	case OP_NOGROUP:	break;
          default:
	fatal("syntax error, operator expected", "");
  }
  if ((t == OP_PRINT) || (t == OP_PRINT0) || (t == OP_EXEC) || (t == OP_OK))
	needprint = 0;

  return p;
}

/*######################## DIAGNOSTICS ##############################*/

void nonfatal(s1, s2)
char *s1, *s2;
{
  fprintf(stderr, "%s: %s%s\n", prog, s1, s2);
}

void fatal(s1, s2)
char *s1, *s2;
{
  nonfatal(s1, s2);
  exit(1);
}

/*################### SMATCH #########################*/
/* Don't try to understand the following one... */
int smatch(s, t)		/* shell-like matching */
char *s, *t;
{
  register n;

  if (*t == '\0') return *s == '\0';
  if (*t == '*') {
	++t;
	do
		if (smatch(s, t)) return 1;
	while (*s++ != '\0');
	return 0;
  }
  if (*s == '\0') return 0;
  if (*t == '\\') return (*s == *++t) ? smatch(++s, ++t) : 0;
  if (*t == '?') return smatch(++s, ++t);
  if (*t == '[') {
	while (*++t != ']') {
		if (*t == '\\') ++t;
		if (*(t + 1) != '-')
			if (*t == *s) {
				while (*++t != ']')
					if (*t == '\\') ++t;
				return smatch(++s, ++t);
			} else
				continue;
		if (*(t + 2) == ']') return(*s == *t || *s == '-');
		n = (*(t + 2) == '\\') ? 3 : 2;
		if (*s >= *t && *s <= *(t + n)) {
			while (*++t != ']')
				if (*t == '\\') ++t;
			return smatch(++s, ++t);
		}
		t += n;
	}
	return 0;
  }
  return(*s == *t) ? smatch(++s, ++t) : 0;
}

/*####################### EXECUTE ###########################*/
/* Do -exec or -ok */

char *
 find_bin(s)
char *s;
{
  char *f, *l, buf[PATH_MAX];

  if (*s == '/')		/* absolute path name */
	return(access(s, 1) == 0) ? s : 0;
  l = f = epath;
  for (;;) {
	if (*l == ':' || *l == 0) {
		if (l == f) {
			if (access(s, 1) == 0) return Salloc(s);
			f++;
		} else {
			register char *p = buf, *q = s;

			while (f != l) *p++ = *f++;
			f++;
			*p++ = '/';
			while (*p++ = *q++) {
			}
			if (access(buf, 1) == 0) return Salloc(buf);
		}
		if (*l == 0) break;
	}
	l++;
  }
  return 0;
}

int execute(op, e, path)
int op;
struct exec *e;
char *path;
{
  int s, pid;
  char *argv[MAXARG];
  register char **p, **q;

  for (p = e->e_vec, q = argv; *p;)	/* replace the {}s */
	if ((*q++ = *p++) == (char *) -1) q[-1] = path;
  *q = '\0';
  if (op == OP_OK) {
	char answer[10];

	for (p = &argv[2]; *p; p++) {
		write(tty, *p, strlen(*p));
		write(tty, " ", 1);
	}
	write(tty, "? ", 2);
	if (read(tty, answer, 10) < 2 || *answer != 'y') return 0;
  }
  if ((pid = fork()) == -1) fatal("can't fork", "");
  if (pid == 0) {
	register i = 3;

	while (close(i++) == 0) {
	}			/* wow!!! */
	execv(argv[1], &argv[2]);	/* binary itself? */
	execv(argv[0], &argv[1]);	/* shell command? */
	fatal("exec failure: ", argv[1]);	/* none of them! */
	exit(127);
  }
  return wait(&s) == pid && s == 0;
}
