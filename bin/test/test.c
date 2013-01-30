/* $NetBSD: test.c,v 1.39 2012/03/15 02:02:21 joerg Exp $ */

/*
 * test(1); version 7-like  --  author Erik Baalbergen
 * modified by Eric Gisin to be used as built-in.
 * modified by Arnold Robbins to add SVR3 compatibility
 * (-x -c -b -p -u -g -k) plus Korn's -L -nt -ot -ef and new -S (socket).
 * modified by J.T. Conklin for NetBSD.
 *
 * This program is in the Public Domain.
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: test.c,v 1.39 2012/03/15 02:02:21 joerg Exp $");
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

/* test(1) accepts the following grammar:
	oexpr	::= aexpr | aexpr "-o" oexpr ;
	aexpr	::= nexpr | nexpr "-a" aexpr ;
	nexpr	::= primary | "!" primary
	primary	::= unary-operator operand
		| operand binary-operator operand
		| operand
		| "(" oexpr ")"
		;
	unary-operator ::= "-r"|"-w"|"-x"|"-f"|"-d"|"-c"|"-b"|"-p"|
		"-u"|"-g"|"-k"|"-s"|"-t"|"-z"|"-n"|"-o"|"-O"|"-G"|"-L"|"-S";

	binary-operator ::= "="|"!="|"-eq"|"-ne"|"-ge"|"-gt"|"-le"|"-lt"|
			"-nt"|"-ot"|"-ef";
	operand ::= <any legal UNIX file name>
*/

enum token {
	EOI,
	FILRD,
	FILWR,
	FILEX,
	FILEXIST,
	FILREG,
	FILDIR,
	FILCDEV,
	FILBDEV,
	FILFIFO,
	FILSOCK,
	FILSYM,
	FILGZ,
	FILTT,
	FILSUID,
	FILSGID,
	FILSTCK,
	FILNT,
	FILOT,
	FILEQ,
	FILUID,
	FILGID,
	STREZ,
	STRNZ,
	STREQ,
	STRNE,
	STRLT,
	STRGT,
	INTEQ,
	INTNE,
	INTGE,
	INTGT,
	INTLE,
	INTLT,
	UNOT,
	BAND,
	BOR,
	LPAREN,
	RPAREN,
	OPERAND
};

enum token_types {
	UNOP,
	BINOP,
	BUNOP,
	BBINOP,
	PAREN
};

struct t_op {
	const char *op_text;
	short op_num, op_type;
};

static const struct t_op cop[] = {
	{"!",	UNOT,	BUNOP},
	{"(",	LPAREN,	PAREN},
	{")",	RPAREN,	PAREN},
	{"<",	STRLT,	BINOP},
	{"=",	STREQ,	BINOP},
	{">",	STRGT,	BINOP},
};

static const struct t_op cop2[] = {
	{"!=",	STRNE,	BINOP},
};

static const struct t_op mop3[] = {
	{"ef",	FILEQ,	BINOP},
	{"eq",	INTEQ,	BINOP},
	{"ge",	INTGE,	BINOP},
	{"gt",	INTGT,	BINOP},
	{"le",	INTLE,	BINOP},
	{"lt",	INTLT,	BINOP},
	{"ne",	INTNE,	BINOP},
	{"nt",	FILNT,	BINOP},
	{"ot",	FILOT,	BINOP},
};

static const struct t_op mop2[] = {
	{"G",	FILGID,	UNOP},
	{"L",	FILSYM,	UNOP},
	{"O",	FILUID,	UNOP},
	{"S",	FILSOCK,UNOP},
	{"a",	BAND,	BBINOP},
	{"b",	FILBDEV,UNOP},
	{"c",	FILCDEV,UNOP},
	{"d",	FILDIR,	UNOP},
	{"e",	FILEXIST,UNOP},
	{"f",	FILREG,	UNOP},
	{"g",	FILSGID,UNOP},
	{"h",	FILSYM,	UNOP},		/* for backwards compat */
	{"k",	FILSTCK,UNOP},
	{"n",	STRNZ,	UNOP},
	{"o",	BOR,	BBINOP},
	{"p",	FILFIFO,UNOP},
	{"r",	FILRD,	UNOP},
	{"s",	FILGZ,	UNOP},
	{"t",	FILTT,	UNOP},
	{"u",	FILSUID,UNOP},
	{"w",	FILWR,	UNOP},
	{"x",	FILEX,	UNOP},
	{"z",	STREZ,	UNOP},
};

static char **t_wp;
static struct t_op const *t_wp_op;

__dead static void syntax(const char *, const char *);
static int oexpr(enum token);
static int aexpr(enum token);
static int nexpr(enum token);
static int primary(enum token);
static int binop(void);
static int test_access(struct stat *, mode_t);
static int filstat(char *, enum token);
static enum token t_lex(char *);
static int isoperand(void);
static long long getn(const char *);
static int newerf(const char *, const char *);
static int olderf(const char *, const char *);
static int equalf(const char *, const char *);

#if defined(SHELL)
extern void error(const char *, ...) __dead __printflike(1, 2);
extern void *ckmalloc(size_t);
#else
static void error(const char *, ...) __dead __printflike(1, 2);

static void
error(const char *msg, ...)
{
	va_list ap;

	va_start(ap, msg);
	verrx(2, msg, ap);
	/*NOTREACHED*/
	va_end(ap);
}

static void *ckmalloc(size_t);
static void *
ckmalloc(size_t nbytes)
{
	void *p = malloc(nbytes);

	if (!p)
		error("Not enough memory!");
	return p;
}
#endif

#ifdef SHELL
int testcmd(int, char **);

int
testcmd(int argc, char **argv)
#else
int main(int, char *[]);

int
main(int argc, char *argv[])
#endif
{
	int res;
	const char *argv0;

#ifdef SHELL
	argv0 = argv[0];
#else
	setprogname(argv[0]);
	(void)setlocale(LC_ALL, "");
	argv0 = getprogname();
#endif
	if (strcmp(argv0, "[") == 0) {
		if (strcmp(argv[--argc], "]"))
			error("missing ]");
		argv[argc] = NULL;
	}

	if (argc < 2)
		return 1;

	t_wp = &argv[1];
	res = !oexpr(t_lex(*t_wp));

	if (*t_wp != NULL && *++t_wp != NULL)
		syntax(*t_wp, "unexpected operator");

	return res;
}

static void
syntax(const char *op, const char *msg)
{

	if (op && *op)
		error("%s: %s", op, msg);
	else
		error("%s", msg);
}

static int
oexpr(enum token n)
{
	int res;

	res = aexpr(n);
	if (*t_wp == NULL)
		return res;
	if (t_lex(*++t_wp) == BOR)
		return oexpr(t_lex(*++t_wp)) || res;
	t_wp--;
	return res;
}

static int
aexpr(enum token n)
{
	int res;

	res = nexpr(n);
	if (*t_wp == NULL)
		return res;
	if (t_lex(*++t_wp) == BAND)
		return aexpr(t_lex(*++t_wp)) && res;
	t_wp--;
	return res;
}

static int
nexpr(enum token n)
{

	if (n == UNOT)
		return !nexpr(t_lex(*++t_wp));
	return primary(n);
}

static int
primary(enum token n)
{
	enum token nn;
	int res;

	if (n == EOI)
		return 0;		/* missing expression */
	if (n == LPAREN) {
		if ((nn = t_lex(*++t_wp)) == RPAREN)
			return 0;	/* missing expression */
		res = oexpr(nn);
		if (t_lex(*++t_wp) != RPAREN)
			syntax(NULL, "closing paren expected");
		return res;
	}
	if (t_wp_op && t_wp_op->op_type == UNOP) {
		/* unary expression */
		if (*++t_wp == NULL)
			syntax(t_wp_op->op_text, "argument expected");
		switch (n) {
		case STREZ:
			return strlen(*t_wp) == 0;
		case STRNZ:
			return strlen(*t_wp) != 0;
		case FILTT:
			return isatty((int)getn(*t_wp));
		default:
			return filstat(*t_wp, n);
		}
	}

	if (t_lex(t_wp[1]), t_wp_op && t_wp_op->op_type == BINOP) {
		return binop();
	}	  

	return strlen(*t_wp) > 0;
}

static int
binop(void)
{
	const char *opnd1, *opnd2;
	struct t_op const *op;

	opnd1 = *t_wp;
	(void) t_lex(*++t_wp);
	op = t_wp_op;

	if ((opnd2 = *++t_wp) == NULL)
		syntax(op->op_text, "argument expected");
		
	switch (op->op_num) {
	case STREQ:
		return strcmp(opnd1, opnd2) == 0;
	case STRNE:
		return strcmp(opnd1, opnd2) != 0;
	case STRLT:
		return strcmp(opnd1, opnd2) < 0;
	case STRGT:
		return strcmp(opnd1, opnd2) > 0;
	case INTEQ:
		return getn(opnd1) == getn(opnd2);
	case INTNE:
		return getn(opnd1) != getn(opnd2);
	case INTGE:
		return getn(opnd1) >= getn(opnd2);
	case INTGT:
		return getn(opnd1) > getn(opnd2);
	case INTLE:
		return getn(opnd1) <= getn(opnd2);
	case INTLT:
		return getn(opnd1) < getn(opnd2);
	case FILNT:
		return newerf(opnd1, opnd2);
	case FILOT:
		return olderf(opnd1, opnd2);
	case FILEQ:
		return equalf(opnd1, opnd2);
	default:
		abort();
		/* NOTREACHED */
	}
}

/*
 * The manual, and IEEE POSIX 1003.2, suggests this should check the mode bits,
 * not use access():
 *
 *	True shall indicate only that the write flag is on.  The file is not
 *	writable on a read-only file system even if this test indicates true.
 *
 * Unfortunately IEEE POSIX 1003.1-2001, as quoted in SuSv3, says only:
 *
 *	True shall indicate that permission to read from file will be granted,
 *	as defined in "File Read, Write, and Creation".
 *
 * and that section says:
 *
 *	When a file is to be read or written, the file shall be opened with an
 *	access mode corresponding to the operation to be performed.  If file
 *	access permissions deny access, the requested operation shall fail.
 *
 * and of course access permissions are described as one might expect:
 *
 *     * If a process has the appropriate privilege:
 *
 *        * If read, write, or directory search permission is requested,
 *          access shall be granted.
 *
 *        * If execute permission is requested, access shall be granted if
 *          execute permission is granted to at least one user by the file
 *          permission bits or by an alternate access control mechanism;
 *          otherwise, access shall be denied.
 *
 *   * Otherwise:
 *
 *        * The file permission bits of a file contain read, write, and
 *          execute/search permissions for the file owner class, file group
 *          class, and file other class.
 *
 *        * Access shall be granted if an alternate access control mechanism
 *          is not enabled and the requested access permission bit is set for
 *          the class (file owner class, file group class, or file other class)
 *          to which the process belongs, or if an alternate access control
 *          mechanism is enabled and it allows the requested access; otherwise,
 *          access shall be denied.
 *
 * and when I first read this I thought:  surely we can't go about using
 * open(O_WRONLY) to try this test!  However the POSIX 1003.1-2001 Rationale
 * section for test does in fact say:
 *
 *	On historical BSD systems, test -w directory always returned false
 *	because test tried to open the directory for writing, which always
 *	fails.
 *
 * and indeed this is in fact true for Seventh Edition UNIX, UNIX 32V, and UNIX
 * System III, and thus presumably also for BSD up to and including 4.3.
 *
 * Secondly I remembered why using open() and/or access() are bogus.  They
 * don't work right for detecting read and write permissions bits when called
 * by root.
 *
 * Interestingly the 'test' in 4.4BSD was closer to correct (as per
 * 1003.2-1992) and it was implemented efficiently with stat() instead of
 * open().
 *
 * This was apparently broken in NetBSD around about 1994/06/30 when the old
 * 4.4BSD implementation was replaced with a (arguably much better coded)
 * implementation derived from pdksh.
 *
 * Note that modern pdksh is yet different again, but still not correct, at
 * least not w.r.t. 1003.2-1992.
 *
 * As I think more about it and read more of the related IEEE docs I don't like
 * that wording about 'test -r' and 'test -w' in 1003.1-2001 at all.  I very
 * much prefer the original wording in 1003.2-1992.  It is much more useful,
 * and so that's what I've implemented.
 *
 * (Note that a strictly conforming implementation of 1003.1-2001 is in fact
 * totally useless for the case in question since its 'test -w' and 'test -r'
 * can never fail for root for any existing files, i.e. files for which 'test
 * -e' succeeds.)
 * 
 * The rationale for 1003.1-2001 suggests that the wording was "clarified" in
 * 1003.1-2001 to align with the 1003.2b draft.  1003.2b Draft 12 (July 1999),
 * which is the latest copy I have, does carry the same suggested wording as is
 * in 1003.1-2001, with its rationale saying:
 * 
 * 	This change is a clarification and is the result of interpretation
 * 	request PASC 1003.2-92 #23 submitted for IEEE Std 1003.2-1992.
 * 
 * That interpretation can be found here:
 * 
 *   http://www.pasc.org/interps/unofficial/db/p1003.2/pasc-1003.2-23.html
 * 
 * Not terribly helpful, unfortunately.  I wonder who that fence sitter was.
 * 
 * Worse, IMVNSHO, I think the authors of 1003.2b-D12 have mis-interpreted the
 * PASC interpretation and appear to be gone against at least one widely used
 * implementation (namely 4.4BSD).  The problem is that for file access by root
 * this means that if test '-r' and '-w' are to behave as if open() were called
 * then there's no way for a shell script running as root to check if a file
 * has certain access bits set other than by the grotty means of interpreting
 * the output of 'ls -l'.  This was widely considered to be a bug in V7's
 * "test" and is, I believe, one of the reasons why direct use of access() was
 * avoided in some more recent implementations!
 * 
 * I have always interpreted '-r' to match '-w' and '-x' as per the original
 * wording in 1003.2-1992, not the other way around.  I think 1003.2b goes much
 * too far the wrong way without any valid rationale and that it's best if we
 * stick with 1003.2-1992 and test the flags, and not mimic the behaviour of
 * open() since we already know very well how it will work -- existance of the
 * file is all that matters to open() for root.
 * 
 * Unfortunately the SVID is no help at all (which is, I guess, partly why
 * we're in this mess in the first place :-).
 * 
 * The SysV implementation (at least in the 'test' builtin in /bin/sh) does use
 * access(name, 2) even though it also goes to much greater lengths for '-x'
 * matching the 1003.2-1992 definition (which is no doubt where that definition
 * came from).
 *
 * The ksh93 implementation uses access() for '-r' and '-w' if
 * (euid==uid&&egid==gid), but uses st_mode for '-x' iff running as root.
 * i.e. it does strictly conform to 1003.1-2001 (and presumably 1003.2b).
 */
static int
test_access(struct stat *sp, mode_t stmode)
{
	gid_t *groups; 
	register int n;
	uid_t euid;
	int maxgroups;

	/*
	 * I suppose we could use access() if not running as root and if we are
	 * running with ((euid == uid) && (egid == gid)), but we've already
	 * done the stat() so we might as well just test the permissions
	 * directly instead of asking the kernel to do it....
	 */
	euid = geteuid();
	if (euid == 0)				/* any bit is good enough */
		stmode = (stmode << 6) | (stmode << 3) | stmode;
 	else if (sp->st_uid == euid)
		stmode <<= 6;
	else if (sp->st_gid == getegid())
		stmode <<= 3;
	else {
		/* XXX stolen almost verbatim from ksh93.... */
		/* on some systems you can be in several groups */
		if ((maxgroups = getgroups(0, NULL)) <= 0)
			maxgroups = NGROUPS_MAX;	/* pre-POSIX system? */
		groups = ckmalloc((maxgroups + 1) * sizeof(gid_t));
		n = getgroups(maxgroups, groups);
		while (--n >= 0) {
			if (groups[n] == sp->st_gid) {
				stmode <<= 3;
				break;
			}
		}
		free(groups);
	}

	return sp->st_mode & stmode;
}

static int
filstat(char *nm, enum token mode)
{
	struct stat s;

	if (mode == FILSYM ? lstat(nm, &s) : stat(nm, &s))
		return 0;

	switch (mode) {
	case FILRD:
		return test_access(&s, S_IROTH);
	case FILWR:
		return test_access(&s, S_IWOTH);
	case FILEX:
		return test_access(&s, S_IXOTH);
	case FILEXIST:
		return 1; /* the successful lstat()/stat() is good enough */
	case FILREG:
		return S_ISREG(s.st_mode);
	case FILDIR:
		return S_ISDIR(s.st_mode);
	case FILCDEV:
		return S_ISCHR(s.st_mode);
	case FILBDEV:
		return S_ISBLK(s.st_mode);
	case FILFIFO:
		return S_ISFIFO(s.st_mode);
	case FILSOCK:
		return S_ISSOCK(s.st_mode);
	case FILSYM:
		return S_ISLNK(s.st_mode);
	case FILSUID:
		return (s.st_mode & S_ISUID) != 0;
	case FILSGID:
		return (s.st_mode & S_ISGID) != 0;
	case FILSTCK:
		return (s.st_mode & S_ISVTX) != 0;
	case FILGZ:
		return s.st_size > (off_t)0;
	case FILUID:
		return s.st_uid == geteuid();
	case FILGID:
		return s.st_gid == getegid();
	default:
		return 1;
	}
}

#define VTOC(x)	(const unsigned char *)((const struct t_op *)x)->op_text

static int
compare1(const void *va, const void *vb)
{
	const unsigned char *a = va;
	const unsigned char *b = VTOC(vb);

	return a[0] - b[0];
}

static int
compare2(const void *va, const void *vb)
{
	const unsigned char *a = va;
	const unsigned char *b = VTOC(vb);
	int z = a[0] - b[0];

	return z ? z : (a[1] - b[1]);
}

static struct t_op const *
findop(const char *s)
{
	if (s[0] == '-') {
		if (s[1] == '\0')
			return NULL;
		if (s[2] == '\0')
			return bsearch(s + 1, mop2, __arraycount(mop2),
			    sizeof(*mop2), compare1);
		else if (s[3] != '\0')
			return NULL;
		else
			return bsearch(s + 1, mop3, __arraycount(mop3),
			    sizeof(*mop3), compare2);
	} else {
		if (s[1] == '\0')
			return bsearch(s, cop, __arraycount(cop), sizeof(*cop),
			    compare1);
		else if (strcmp(s, cop2[0].op_text) == 0)
			return cop2;
		else
			return NULL;
	}
}

static enum token
t_lex(char *s)
{
	struct t_op const *op;

	if (s == NULL) {
		t_wp_op = NULL;
		return EOI;
	}

	if ((op = findop(s)) != NULL) {
		if (!((op->op_type == UNOP && isoperand()) ||
		    (op->op_num == LPAREN && *(t_wp+1) == 0))) {
			t_wp_op = op;
			return op->op_num;
		}
	}
	t_wp_op = NULL;
	return OPERAND;
}

static int
isoperand(void)
{
	struct t_op const *op;
	char *s, *t;

	if ((s  = *(t_wp+1)) == 0)
		return 1;
	if ((t = *(t_wp+2)) == 0)
		return 0;
	if ((op = findop(s)) != NULL)
		return op->op_type == BINOP && (t[0] != ')' || t[1] != '\0'); 
	return 0;
}

/* atoi with error detection */
static long long
getn(const char *s)
{
	char *p;
	long long r;

	errno = 0;
	r = strtoll(s, &p, 10);

	if (errno != 0)
	if (errno == ERANGE && (r == LLONG_MAX || r == LLONG_MIN))
	      error("%s: out of range", s);

	while (isspace((unsigned char)*p))
	      p++;
	
	if (*p || p == s)
	      error("%s: bad number", s);

	return r;
}

static int
newerf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
		stat(f2, &b2) == 0 &&
		b1.st_mtime > b2.st_mtime);
}

static int
olderf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
		stat(f2, &b2) == 0 &&
		b1.st_mtime < b2.st_mtime);
}

static int
equalf(const char *f1, const char *f2)
{
	struct stat b1, b2;

	return (stat(f1, &b1) == 0 &&
		stat(f2, &b2) == 0 &&
		b1.st_dev == b2.st_dev &&
		b1.st_ino == b2.st_ino);
}
