/*	$NetBSD: ex_argv.c,v 1.4 2011/03/21 14:53:03 tnozaki Exp $ */

/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "Id: ex_argv.c,v 10.39 2003/11/05 17:11:54 skimo Exp (Berkeley) Date: 2003/11/05 17:11:54";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "../common/common.h"

static int argv_alloc __P((SCR *, size_t));
static int argv_comp __P((const void *, const void *));
static int argv_fexp __P((SCR *, EXCMD *,
	const CHAR_T *, size_t, CHAR_T *, size_t *, CHAR_T **, size_t *, int));
static int argv_lexp __P((SCR *, EXCMD *, const char *));
static int argv_sexp __P((SCR *, CHAR_T **, size_t *, size_t *));

/*
 * argv_init --
 *	Build  a prototype arguments list.
 *
 * PUBLIC: int argv_init __P((SCR *, EXCMD *));
 */
int
argv_init(SCR *sp, EXCMD *excp)
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	exp->argsoff = 0;
	argv_alloc(sp, 1);

	excp->argv = exp->args;
	excp->argc = exp->argsoff;
	return (0);
}

/*
 * argv_exp0 --
 *	Append a string to the argument list.
 *
 * PUBLIC: int argv_exp0 __P((SCR *, EXCMD *, CHAR_T *, size_t));
 */
int
argv_exp0(SCR *sp, EXCMD *excp, const CHAR_T *cmd, size_t cmdlen)
{
	EX_PRIVATE *exp;

	exp = EXP(sp);
	argv_alloc(sp, cmdlen);
	MEMCPY(exp->args[exp->argsoff]->bp, cmd, cmdlen);
	exp->args[exp->argsoff]->bp[cmdlen] = '\0';
	exp->args[exp->argsoff]->len = cmdlen;
	++exp->argsoff;
	excp->argv = exp->args;
	excp->argc = exp->argsoff;
	return (0);
}

/*
 * argv_exp1 --
 *	Do file name expansion on a string, and append it to the
 *	argument list.
 *
 * PUBLIC: int argv_exp1 __P((SCR *, EXCMD *, const CHAR_T *, size_t, int));
 */
int
argv_exp1(SCR *sp, EXCMD *excp, const CHAR_T *cmd, size_t cmdlen, int is_bang)
{
	EX_PRIVATE *exp;
	size_t blen, len;
	CHAR_T *p, *t, *bp;

	GET_SPACE_RETW(sp, bp, blen, 512);

	len = 0;
	exp = EXP(sp);
	if (argv_fexp(sp, excp, cmd, cmdlen, bp, &len, &bp, &blen, is_bang)) {
		FREE_SPACEW(sp, bp, blen);
		return (1);
	}

	/* If it's empty, we're done. */
	if (len != 0) {
		for (p = bp, t = bp + len; p < t; ++p)
			if (!ISBLANK((UCHAR_T)*p))
				break;
		if (p == t)
			goto ret;
	} else
		goto ret;

	(void)argv_exp0(sp, excp, bp, len);

ret:	FREE_SPACEW(sp, bp, blen);
	return (0);
}

/*
 * argv_exp2 --
 *	Do file name and shell expansion on a string, and append it to
 *	the argument list.
 *
 * PUBLIC: int argv_exp2 __P((SCR *, EXCMD *, CHAR_T *, size_t));
 */
int
argv_exp2(SCR *sp, EXCMD *excp, const CHAR_T *cmd, size_t cmdlen)
{
	size_t blen, len, n;
	int rval;
	CHAR_T *bp, *p;
	const char *mp, *np;

	GET_SPACE_RETW(sp, bp, blen, 512);

#define	SHELLECHO	"echo "
#define	SHELLOFFSET	(sizeof(SHELLECHO) - 1)
	p = bp;
	*p++ = 'e'; 
	*p++ = 'c'; 
	*p++ = 'h'; 
	*p++ = 'o'; 
	*p++ = ' ';
	len = SHELLOFFSET;

#if defined(DEBUG) && 0
	vtrace(sp, "file_argv: {%.*s}\n", (int)cmdlen, cmd);
#endif

	if (argv_fexp(sp, excp, cmd, cmdlen, p, &len, &bp, &blen, 0)) {
		rval = 1;
		goto err;
	}

#if defined(DEBUG) && 0
	vtrace(sp, "before shell: %d: {%s}\n", len, bp);
#endif

	/*
	 * Do shell word expansion -- it's very, very hard to figure out what
	 * magic characters the user's shell expects.  Historically, it was a
	 * union of v7 shell and csh meta characters.  We match that practice
	 * by default, so ":read \%" tries to read a file named '%'.  It would
	 * make more sense to pass any special characters through the shell,
	 * but then, if your shell was csh, the above example will behave
	 * differently in nvi than in vi.  If you want to get other characters
	 * passed through to your shell, change the "meta" option.
	 *
	 * To avoid a function call per character, we do a first pass through
	 * the meta characters looking for characters that aren't expected
	 * to be there, and then we can ignore them in the user's argument.
	 */
	if (opts_empty(sp, O_SHELL, 1) || opts_empty(sp, O_SHELLMETA, 1))
		n = 0;
	else {
		for (np = mp = O_STR(sp, O_SHELLMETA); *np != '\0'; ++np)
			if (isblank((unsigned char)*np) ||
			    isalnum((unsigned char)*np))
				break;
		p = bp + SHELLOFFSET;
		n = len - SHELLOFFSET;
		if (*p != '\0') {
			for (; n > 0; --n, ++p)
				if (strchr(mp, *p) != NULL)
					break;
		} else
			for (; n > 0; --n, ++p)
				if (!isblank((unsigned char)*p) &&
				    !isalnum((unsigned char)*p) && strchr(mp, *p) != NULL)
					break;
	}

	/*
	 * If we found a meta character in the string, fork a shell to expand
	 * it.  Unfortunately, this is comparatively slow.  Historically, it
	 * didn't matter much, since users don't enter meta characters as part
	 * of pathnames that frequently.  The addition of filename completion
	 * broke that assumption because it's easy to use.  As a result, lots
	 * folks have complained that the expansion code is too slow.  So, we
	 * detect filename completion as a special case, and do it internally.
	 * Note that this code assumes that the <asterisk> character is the
	 * match-anything meta character.  That feels safe -- if anyone writes
	 * a shell that doesn't follow that convention, I'd suggest giving them
	 * a festive hot-lead enema.
	 */
	switch (n) {
	case 0:
		p = bp + SHELLOFFSET;
		len -= SHELLOFFSET;
		rval = argv_exp3(sp, excp, p, len);
		break;
	case 1:
		if (*p == '*') {
			const char *np1;
			char *d;
			size_t nlen;

			*p = '\0';
			INT2CHAR(sp, bp + SHELLOFFSET, 
				 STRLEN(bp + SHELLOFFSET) + 1, np1, nlen);
			d = strdup(np1);
			rval = argv_lexp(sp, excp, d);
			free (d);
			break;
		}
		/* FALLTHROUGH */
	default:
		if (argv_sexp(sp, &bp, &blen, &len)) {
			rval = 1;
			goto err;
		}
		p = bp;
		rval = argv_exp3(sp, excp, p, len);
		break;
	}

err:	FREE_SPACEW(sp, bp, blen);
	return (rval);
}

/*
 * argv_exp3 --
 *	Take a string and break it up into an argv, which is appended
 *	to the argument list.
 *
 * PUBLIC: int argv_exp3 __P((SCR *, EXCMD *, CHAR_T *, size_t));
 */
int
argv_exp3(SCR *sp, EXCMD *excp, const CHAR_T *cmd, size_t cmdlen)
{
	EX_PRIVATE *exp;
	size_t len;
	ARG_CHAR_T ch;
	int off;
	const CHAR_T *ap;
	CHAR_T *p;

	for (exp = EXP(sp); cmdlen > 0; ++exp->argsoff) {
		/* Skip any leading whitespace. */
		for (; cmdlen > 0; --cmdlen, ++cmd) {
			ch = (UCHAR_T)*cmd;
			if (!ISBLANK(ch))
				break;
		}
		if (cmdlen == 0)
			break;

		/*
		 * Determine the length of this whitespace delimited
		 * argument.
		 *
		 * QUOTING NOTE:
		 *
		 * Skip any character preceded by the user's quoting
		 * character.
		 */
		for (ap = cmd, len = 0; cmdlen > 0; ++cmd, --cmdlen, ++len) {
			ch = (UCHAR_T)*cmd;
			if (IS_ESCAPE(sp, excp, ch) && cmdlen > 1) {
				++cmd;
				--cmdlen;
			} else if (ISBLANK(ch))
				break;
		}

		/*
		 * Copy the argument into place.
		 *
		 * QUOTING NOTE:
		 *
		 * Lose quote chars.
		 */
		argv_alloc(sp, len);
		off = exp->argsoff;
		exp->args[off]->len = len;
		for (p = exp->args[off]->bp; len > 0; --len, *p++ = *ap++)
			if (IS_ESCAPE(sp, excp, *ap))
				++ap;
		*p = '\0';
	}
	excp->argv = exp->args;
	excp->argc = exp->argsoff;

#if defined(DEBUG) && 0
	for (cnt = 0; cnt < exp->argsoff; ++cnt)
		vtrace(sp, "arg %d: {%s}\n", cnt, exp->argv[cnt]);
#endif
	return (0);
}

/*
 * argv_fexp --
 *	Do file name and bang command expansion.
 */
static int
argv_fexp(SCR *sp, EXCMD *excp, const CHAR_T *cmd, size_t cmdlen, CHAR_T *p, size_t *lenp, CHAR_T **bpp, size_t *blenp, int is_bang)
{
	EX_PRIVATE *exp;
	char *t;
	size_t blen, len, off, tlen;
	CHAR_T *bp;
	const CHAR_T *wp;
	size_t wlen;

	/* Replace file name characters. */
	for (bp = *bpp, blen = *blenp, len = *lenp; cmdlen > 0; --cmdlen, ++cmd)
		switch (*cmd) {
		case '!':
			if (!is_bang)
				goto ins_ch;
			exp = EXP(sp);
			if (exp->lastbcomm == NULL) {
				msgq(sp, M_ERR,
				    "115|No previous command to replace \"!\"");
				return (1);
			}
			len += tlen = STRLEN(exp->lastbcomm);
			off = p - bp;
			ADD_SPACE_RETW(sp, bp, blen, len);
			p = bp + off;
			MEMCPY(p, exp->lastbcomm, tlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '%':
			if ((t = sp->frp->name) == NULL) {
				msgq(sp, M_ERR,
				    "116|No filename to substitute for %%");
				return (1);
			}
			tlen = strlen(t);
			len += tlen;
			off = p - bp;
			ADD_SPACE_RETW(sp, bp, blen, len);
			p = bp + off;
			CHAR2INT(sp, t, tlen, wp, wlen);
			MEMCPY(p, wp, wlen);
			p += wlen;
			F_SET(excp, E_MODIFY);
			break;
		case '#':
			if ((t = sp->alt_name) == NULL) {
				msgq(sp, M_ERR,
				    "117|No filename to substitute for #");
				return (1);
			}
			len += tlen = strlen(t);
			off = p - bp;
			ADD_SPACE_RETW(sp, bp, blen, len);
			p = bp + off;
			CHAR2INT(sp, t, tlen, wp, wlen);
			MEMCPY(p, wp, wlen);
			p += tlen;
			F_SET(excp, E_MODIFY);
			break;
		case '\\':
			/*
			 * QUOTING NOTE:
			 *
			 * Strip any backslashes that protected the file
			 * expansion characters.
			 */
			if (cmdlen > 1 &&
			    (cmd[1] == '%' || cmd[1] == '#' || cmd[1] == '!')) {
				++cmd;
				--cmdlen;
			}
			/* FALLTHROUGH */
		default:
ins_ch:			++len;
			off = p - bp;
			ADD_SPACE_RETW(sp, bp, blen, len);
			p = bp + off;
			*p++ = *cmd;
		}

	/* Nul termination. */
	++len;
	off = p - bp;
	ADD_SPACE_RETW(sp, bp, blen, len);
	p = bp + off;
	*p = '\0';

	/* Return the new string length, buffer, buffer length. */
	*lenp = len - 1;
	*bpp = bp;
	*blenp = blen;
	return (0);
}

/*
 * argv_alloc --
 *	Make more space for arguments.
 */
static int
argv_alloc(SCR *sp, size_t len)
{
	ARGS *ap;
	EX_PRIVATE *exp;
	int cnt, off;

	/*
	 * Allocate room for another argument, always leaving
	 * enough room for an ARGS structure with a length of 0.
	 */
#define	INCREMENT	20
	exp = EXP(sp);
	off = exp->argsoff;
	if (exp->argscnt == 0 || off + 2 >= exp->argscnt - 1) {
		cnt = exp->argscnt + INCREMENT;
		REALLOC(sp, exp->args, ARGS **, cnt * sizeof(ARGS *));
		if (exp->args == NULL) {
			(void)argv_free(sp);
			goto mem;
		}
		memset(&exp->args[exp->argscnt], 0, INCREMENT * sizeof(ARGS *));
		exp->argscnt = cnt;
	}

	/* First argument. */
	if (exp->args[off] == NULL) {
		CALLOC(sp, exp->args[off], ARGS *, 1, sizeof(ARGS));
		if (exp->args[off] == NULL)
			goto mem;
	}

	/* First argument buffer. */
	ap = exp->args[off];
	ap->len = 0;
	if (ap->blen < len + 1) {
		ap->blen = len + 1;
		REALLOC(sp, ap->bp, CHAR_T *, ap->blen * sizeof(CHAR_T));
		if (ap->bp == NULL) {
			ap->bp = NULL;
			ap->blen = 0;
			F_CLR(ap, A_ALLOCATED);
mem:			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
		F_SET(ap, A_ALLOCATED);
	}

	/* Second argument. */
	if (exp->args[++off] == NULL) {
		CALLOC(sp, exp->args[off], ARGS *, 1, sizeof(ARGS));
		if (exp->args[off] == NULL)
			goto mem;
	}
	/* 0 length serves as end-of-argument marker. */
	exp->args[off]->len = 0;
	return (0);
}

/*
 * argv_free --
 *	Free up argument structures.
 *
 * PUBLIC: int argv_free __P((SCR *));
 */
int
argv_free(SCR *sp)
{
	EX_PRIVATE *exp;
	int off;

	exp = EXP(sp);
	if (exp->args != NULL) {
		for (off = 0; off < exp->argscnt; ++off) {
			if (exp->args[off] == NULL)
				continue;
			if (F_ISSET(exp->args[off], A_ALLOCATED))
				free(exp->args[off]->bp);
			free(exp->args[off]);
		}
		free(exp->args);
	}
	exp->args = NULL;
	exp->argscnt = 0;
	exp->argsoff = 0;
	return (0);
}

/*
 * argv_lexp --
 *	Find all file names matching the prefix and append them to the
 *	buffer.
 */
static int
argv_lexp(SCR *sp, EXCMD *excp, const char *path)
{
	struct dirent *dp;
	DIR *dirp;
	EX_PRIVATE *exp;
	int off;
	size_t dlen, len, nlen;
	const char *dname, *name;
	char *p;
	size_t wlen;
	const CHAR_T *wp;
	CHAR_T *n;

	exp = EXP(sp);

	/* Set up the name and length for comparison. */
	if ((p = strrchr(path, '/')) == NULL) {
		dname = ".";
		dlen = 0;
		name = path;
	} else { 
		if (p == path) {
			dname = "/";
			dlen = 1;
		} else {
			*p = '\0';
			dname = path;
			dlen = strlen(path);
		}
		name = p + 1;
	}
	nlen = strlen(name);

	/*
	 * XXX
	 * We don't use the d_namlen field, it's not portable enough; we
	 * assume that d_name is nul terminated, instead.
	 */
	if ((dirp = opendir(dname)) == NULL) {
		msgq_str(sp, M_SYSERR, dname, "%s");
		return (1);
	}
	for (off = exp->argsoff; (dp = readdir(dirp)) != NULL;) {
		if (nlen == 0) {
			if (dp->d_name[0] == '.')
				continue;
			len = strlen(dp->d_name);
		} else {
			len = strlen(dp->d_name);
			if (len < nlen || memcmp(dp->d_name, name, nlen))
				continue;
		}

		/* Directory + name + slash + null. */
		argv_alloc(sp, dlen + len + 2);
		n = exp->args[exp->argsoff]->bp;
		if (dlen != 0) {
			CHAR2INT(sp, dname, dlen, wp, wlen);
			MEMCPY(n, wp, wlen);
			n += dlen;
			if (dlen > 1 || dname[0] != '/')
				*n++ = '/';
		}
		CHAR2INT(sp, dp->d_name, len + 1, wp, wlen);
		MEMCPY(n, wp, wlen);
		exp->args[exp->argsoff]->len = dlen + len + 1;
		++exp->argsoff;
		excp->argv = exp->args;
		excp->argc = exp->argsoff;
	}
	closedir(dirp);

	if (off == exp->argsoff) {
		/*
		 * If we didn't find a match, complain that the expansion
		 * failed.  We can't know for certain that's the error, but
		 * it's a good guess, and it matches historic practice. 
		 */
		msgq(sp, M_ERR, "304|Shell expansion failed");
		return (1);
	}
	qsort(exp->args + off, exp->argsoff - off, sizeof(ARGS *), argv_comp);
	return (0);
}

/*
 * argv_comp --
 *	Alphabetic comparison.
 */
static int
argv_comp(const void *a, const void *b)
{
	return (STRCMP((*(const ARGS * const*)a)->bp, (*(const ARGS * const*)b)->bp));
}

static pid_t
runcmd(SCR *sp, const char *sh_path, const char *sh, const char *np,
    int *std_output)
{
	pid_t pid;
	/*
	 * Do the minimal amount of work possible, the shell is going to run
	 * briefly and then exit.  We sincerely hope.
	 */
	switch (pid = vfork()) {
	case -1:			/* Error. */
		msgq(sp, M_SYSERR, "vfork");
		return (pid_t)-1;
	case 0:				/* Utility. */
		/* Redirect stdout to the write end of the pipe. */
		(void)dup2(std_output[1], STDOUT_FILENO);

		/* Close the utility's file descriptors. */
		(void)close(std_output[0]);
		(void)close(std_output[1]);
		(void)close(STDERR_FILENO);

		/*
		 * XXX
		 * Assume that all shells have -c.
		 */
		execl(sh_path, sh, "-c", np, (char *)NULL);
		msgq_str(sp, M_SYSERR, sh_path, "118|Error: execl: %s");
		_exit(127);
	default:			/* Parent. */
		/* Close the pipe ends the parent won't use. */
		(void)close(std_output[1]);
		return pid;
	}
}

/*
 * argv_sexp --
 *	Fork a shell, pipe a command through it, and read the output into
 *	a buffer.
 */
static int
argv_sexp(SCR *sp, CHAR_T **bpp, size_t *blenp, size_t *lenp)
{
	enum { SEXP_ERR, SEXP_EXPANSION_ERR, SEXP_OK } rval;
	FILE *ifp;
	pid_t pid;
	size_t blen, len;
	int ch, std_output[2];
	CHAR_T *bp, *p;
	const char *sh, *sh_path;
	const char *np;
	size_t nlen;

	/* Secure means no shell access. */
	if (O_ISSET(sp, O_SECURE)) {
		msgq(sp, M_ERR,
"289|Shell expansions not supported when the secure edit option is set");
		return (1);
	}

	sh_path = O_STR(sp, O_SHELL);
	if ((sh = strrchr(sh_path, '/')) == NULL)
		sh = sh_path;
	else
		++sh;

	/* Local copies of the buffer variables. */
	bp = *bpp;
	blen = *blenp;

	/*
	 * There are two different processes running through this code, named
	 * the utility (the shell) and the parent. The utility reads standard
	 * input and writes standard output and standard error output.  The
	 * parent writes to the utility, reads its standard output and ignores
	 * its standard error output.  Historically, the standard error output
	 * was discarded by vi, as it produces a lot of noise when file patterns
	 * don't match.
	 *
	 * The parent reads std_output[0], and the utility writes std_output[1].
	 */
	ifp = NULL;
	std_output[0] = std_output[1] = -1;
	if (pipe(std_output) < 0) {
		msgq(sp, M_SYSERR, "pipe");
		return (1);
	}
	if ((ifp = fdopen(std_output[0], "r")) == NULL) {
		msgq(sp, M_SYSERR, "fdopen");
		goto err;
	}
	INT2CHAR(sp, bp, STRLEN(bp)+1, np, nlen);
	pid = runcmd(sp, sh_path, sh, np, std_output);
	if (pid == -1)
		goto err;

	/*
	 * Copy process standard output into a buffer.
	 *
	 * !!!
	 * Historic vi apparently discarded leading \n and \r's from
	 * the shell output stream.  We don't on the grounds that any
	 * shell that does that is broken.
	 */
	for (p = bp, len = 0, ch = EOF;
	    (ch = getc(ifp)) != EOF; *p++ = ch, blen-=sizeof(CHAR_T), ++len)
		if (blen < 5) {
			ADD_SPACE_GOTOW(sp, bp, *blenp, *blenp * 2);
			p = bp + len;
			blen = *blenp - len;
		}

	/* Delete the final newline, nul terminate the string. */
	if (p > bp && (p[-1] == '\n' || p[-1] == '\r')) {
		--p;
		--len;
	}
	*p = '\0';
	*lenp = len;
	*bpp = bp;		/* *blenp is already updated. */

	if (ferror(ifp))
		goto ioerr;
	if (fclose(ifp)) {
ioerr:		msgq_str(sp, M_ERR, sh, "119|I/O error: %s");
alloc_err:	rval = SEXP_ERR;
	} else
		rval = SEXP_OK;

	/*
	 * Wait for the process.  If the shell process fails (e.g., "echo $q"
	 * where q wasn't a defined variable) or if the returned string has
	 * no characters or only blank characters, (e.g., "echo $5"), complain
	 * that the shell expansion failed.  We can't know for certain that's
	 * the error, but it's a good guess, and it matches historic practice.
	 * This won't catch "echo foo_$5", but that's not a common error and
	 * historic vi didn't catch it either.
	 */
	if (proc_wait(sp, (long)pid, sh, 1, 0))
		rval = SEXP_EXPANSION_ERR;

	for (p = bp; len; ++p, --len)
		if (!ISBLANK((UCHAR_T)*p))
			break;
	if (len == 0)
		rval = SEXP_EXPANSION_ERR;

	if (rval == SEXP_EXPANSION_ERR)
		msgq(sp, M_ERR, "304|Shell expansion failed");

	return (rval == SEXP_OK ? 0 : 1);
err:	if (ifp != NULL)
		(void)fclose(ifp);
	else if (std_output[0] != -1)
		close(std_output[0]);
	if (std_output[1] != -1)
		close(std_output[0]);
	return 1;
}
