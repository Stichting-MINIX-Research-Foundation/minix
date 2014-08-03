/*	$NetBSD: exec.c,v 1.45 2013/11/01 16:49:02 christos Exp $	*/

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Kenneth Almquist.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#ifndef lint
#if 0
static char sccsid[] = "@(#)exec.c	8.4 (Berkeley) 6/8/95";
#else
__RCSID("$NetBSD: exec.c,v 1.45 2013/11/01 16:49:02 christos Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * When commands are first encountered, they are entered in a hash table.
 * This ensures that a full path search will not have to be done for them
 * on each invocation.
 *
 * We should investigate converting to a linear search, even though that
 * would make the command name "hash" a misnomer.
 */

#include "shell.h"
#include "main.h"
#include "nodes.h"
#include "parser.h"
#include "redir.h"
#include "eval.h"
#include "exec.h"
#include "builtins.h"
#include "var.h"
#include "options.h"
#include "input.h"
#include "output.h"
#include "syntax.h"
#include "memalloc.h"
#include "error.h"
#include "init.h"
#include "mystring.h"
#include "show.h"
#include "jobs.h"
#include "alias.h"


#define CMDTABLESIZE 31		/* should be prime */
#define ARB 1			/* actual size determined at run time */



struct tblentry {
	struct tblentry *next;	/* next entry in hash chain */
	union param param;	/* definition of builtin function */
	short cmdtype;		/* index identifying command */
	char rehash;		/* if set, cd done since entry created */
	char cmdname[ARB];	/* name of command */
};


STATIC struct tblentry *cmdtable[CMDTABLESIZE];
STATIC int builtinloc = -1;		/* index in path of %builtin, or -1 */
int exerrno = 0;			/* Last exec error */


STATIC void tryexec(char *, char **, char **, int);
STATIC void printentry(struct tblentry *, int);
STATIC void clearcmdentry(int);
STATIC struct tblentry *cmdlookup(const char *, int);
STATIC void delete_cmd_entry(void);

#ifndef BSD
STATIC void execinterp(char **, char **);
#endif


extern const char *const parsekwd[];

/*
 * Exec a program.  Never returns.  If you change this routine, you may
 * have to change the find_command routine as well.
 */

void
shellexec(char **argv, char **envp, const char *path, int idx, int vforked)
{
	char *cmdname;
	int e;

	if (strchr(argv[0], '/') != NULL) {
		tryexec(argv[0], argv, envp, vforked);
		e = errno;
	} else {
		e = ENOENT;
		while ((cmdname = padvance(&path, argv[0])) != NULL) {
			if (--idx < 0 && pathopt == NULL) {
				tryexec(cmdname, argv, envp, vforked);
				if (errno != ENOENT && errno != ENOTDIR)
					e = errno;
			}
			stunalloc(cmdname);
		}
	}

	/* Map to POSIX errors */
	switch (e) {
	case EACCES:
		exerrno = 126;
		break;
	case ENOENT:
		exerrno = 127;
		break;
	default:
		exerrno = 2;
		break;
	}
	TRACE(("shellexec failed for %s, errno %d, vforked %d, suppressint %d\n",
		argv[0], e, vforked, suppressint ));
	exerror(EXEXEC, "%s: %s", argv[0], errmsg(e, E_EXEC));
	/* NOTREACHED */
}


STATIC void
tryexec(char *cmd, char **argv, char **envp, int vforked)
{
	int e;
#ifndef BSD
	char *p;
#endif

#ifdef SYSV
	do {
		execve(cmd, argv, envp);
	} while (errno == EINTR);
#else
	execve(cmd, argv, envp);
#endif
	e = errno;
	if (e == ENOEXEC) {
		if (vforked) {
			/* We are currently vfork(2)ed, so raise an
			 * exception, and evalcommand will try again
			 * with a normal fork(2).
			 */
			exraise(EXSHELLPROC);
		}
#ifdef DEBUG
		TRACE(("execve(cmd=%s) returned ENOEXEC\n", cmd));
#endif
		initshellproc();
		setinputfile(cmd, 0);
		commandname = arg0 = savestr(argv[0]);
#ifndef BSD
		pgetc(); pungetc();		/* fill up input buffer */
		p = parsenextc;
		if (parsenleft > 2 && p[0] == '#' && p[1] == '!') {
			argv[0] = cmd;
			execinterp(argv, envp);
		}
#endif
		setparam(argv + 1);
		exraise(EXSHELLPROC);
	}
	errno = e;
}


#ifndef BSD
/*
 * Execute an interpreter introduced by "#!", for systems where this
 * feature has not been built into the kernel.  If the interpreter is
 * the shell, return (effectively ignoring the "#!").  If the execution
 * of the interpreter fails, exit.
 *
 * This code peeks inside the input buffer in order to avoid actually
 * reading any input.  It would benefit from a rewrite.
 */

#define NEWARGS 5

STATIC void
execinterp(char **argv, char **envp)
{
	int n;
	char *inp;
	char *outp;
	char c;
	char *p;
	char **ap;
	char *newargs[NEWARGS];
	int i;
	char **ap2;
	char **new;

	n = parsenleft - 2;
	inp = parsenextc + 2;
	ap = newargs;
	for (;;) {
		while (--n >= 0 && (*inp == ' ' || *inp == '\t'))
			inp++;
		if (n < 0)
			goto bad;
		if ((c = *inp++) == '\n')
			break;
		if (ap == &newargs[NEWARGS])
bad:		  error("Bad #! line");
		STARTSTACKSTR(outp);
		do {
			STPUTC(c, outp);
		} while (--n >= 0 && (c = *inp++) != ' ' && c != '\t' && c != '\n');
		STPUTC('\0', outp);
		n++, inp--;
		*ap++ = grabstackstr(outp);
	}
	if (ap == newargs + 1) {	/* if no args, maybe no exec is needed */
		p = newargs[0];
		for (;;) {
			if (equal(p, "sh") || equal(p, "ash")) {
				return;
			}
			while (*p != '/') {
				if (*p == '\0')
					goto break2;
				p++;
			}
			p++;
		}
break2:;
	}
	i = (char *)ap - (char *)newargs;		/* size in bytes */
	if (i == 0)
		error("Bad #! line");
	for (ap2 = argv ; *ap2++ != NULL ; );
	new = ckmalloc(i + ((char *)ap2 - (char *)argv));
	ap = newargs, ap2 = new;
	while ((i -= sizeof (char **)) >= 0)
		*ap2++ = *ap++;
	ap = argv;
	while (*ap2++ = *ap++);
	shellexec(new, envp, pathval(), 0);
	/* NOTREACHED */
}
#endif



/*
 * Do a path search.  The variable path (passed by reference) should be
 * set to the start of the path before the first call; padvance will update
 * this value as it proceeds.  Successive calls to padvance will return
 * the possible path expansions in sequence.  If an option (indicated by
 * a percent sign) appears in the path entry then the global variable
 * pathopt will be set to point to it; otherwise pathopt will be set to
 * NULL.
 */

const char *pathopt;

char *
padvance(const char **path, const char *name)
{
	const char *p;
	char *q;
	const char *start;
	int len;

	if (*path == NULL)
		return NULL;
	start = *path;
	for (p = start ; *p && *p != ':' && *p != '%' ; p++);
	len = p - start + strlen(name) + 2;	/* "2" is for '/' and '\0' */
	while (stackblocksize() < len)
		growstackblock();
	q = stackblock();
	if (p != start) {
		memcpy(q, start, p - start);
		q += p - start;
		*q++ = '/';
	}
	strcpy(q, name);
	pathopt = NULL;
	if (*p == '%') {
		pathopt = ++p;
		while (*p && *p != ':')  p++;
	}
	if (*p == ':')
		*path = p + 1;
	else
		*path = NULL;
	return stalloc(len);
}



/*** Command hashing code ***/


int
hashcmd(int argc, char **argv)
{
	struct tblentry **pp;
	struct tblentry *cmdp;
	int c;
	int verbose;
	struct cmdentry entry;
	char *name;

	verbose = 0;
	while ((c = nextopt("rv")) != '\0') {
		if (c == 'r') {
			clearcmdentry(0);
		} else if (c == 'v') {
			verbose++;
		}
	}
	if (*argptr == NULL) {
		for (pp = cmdtable ; pp < &cmdtable[CMDTABLESIZE] ; pp++) {
			for (cmdp = *pp ; cmdp ; cmdp = cmdp->next) {
				if (verbose || cmdp->cmdtype == CMDNORMAL)
					printentry(cmdp, verbose);
			}
		}
		return 0;
	}
	while ((name = *argptr) != NULL) {
		if ((cmdp = cmdlookup(name, 0)) != NULL
		 && (cmdp->cmdtype == CMDNORMAL
		     || (cmdp->cmdtype == CMDBUILTIN && builtinloc >= 0)))
			delete_cmd_entry();
		find_command(name, &entry, DO_ERR, pathval());
		if (verbose) {
			if (entry.cmdtype != CMDUNKNOWN) {	/* if no error msg */
				cmdp = cmdlookup(name, 0);
				if (cmdp != NULL)
					printentry(cmdp, verbose);
			}
			flushall();
		}
		argptr++;
	}
	return 0;
}


STATIC void
printentry(struct tblentry *cmdp, int verbose)
{
	int idx;
	const char *path;
	char *name;

	switch (cmdp->cmdtype) {
	case CMDNORMAL:
		idx = cmdp->param.index;
		path = pathval();
		do {
			name = padvance(&path, cmdp->cmdname);
			stunalloc(name);
		} while (--idx >= 0);
		out1str(name);
		break;
	case CMDSPLBLTIN:
		out1fmt("special builtin %s", cmdp->cmdname);
		break;
	case CMDBUILTIN:
		out1fmt("builtin %s", cmdp->cmdname);
		break;
	case CMDFUNCTION:
		out1fmt("function %s", cmdp->cmdname);
		if (verbose) {
			struct procstat ps;
			INTOFF;
			commandtext(&ps, cmdp->param.func);
			INTON;
			out1str("() { ");
			out1str(ps.cmd);
			out1str("; }");
		}
		break;
	default:
		error("internal error: %s cmdtype %d", cmdp->cmdname, cmdp->cmdtype);
	}
	if (cmdp->rehash)
		out1c('*');
	out1c('\n');
}



/*
 * Resolve a command name.  If you change this routine, you may have to
 * change the shellexec routine as well.
 */

void
find_command(char *name, struct cmdentry *entry, int act, const char *path)
{
	struct tblentry *cmdp, loc_cmd;
	int idx;
	int prev;
	char *fullname;
	struct stat statb;
	int e;
	int (*bltin)(int,char **);

	/* If name contains a slash, don't use PATH or hash table */
	if (strchr(name, '/') != NULL) {
		if (act & DO_ABS) {
			while (stat(name, &statb) < 0) {
#ifdef SYSV
				if (errno == EINTR)
					continue;
#endif
				if (errno != ENOENT && errno != ENOTDIR)
					e = errno;
				entry->cmdtype = CMDUNKNOWN;
				entry->u.index = -1;
				return;
			}
			entry->cmdtype = CMDNORMAL;
			entry->u.index = -1;
			return;
		}
		entry->cmdtype = CMDNORMAL;
		entry->u.index = 0;
		return;
	}

	if (path != pathval())
		act |= DO_ALTPATH;

	if (act & DO_ALTPATH && strstr(path, "%builtin") != NULL)
		act |= DO_ALTBLTIN;

	/* If name is in the table, check answer will be ok */
	if ((cmdp = cmdlookup(name, 0)) != NULL) {
		do {
			switch (cmdp->cmdtype) {
			case CMDNORMAL:
				if (act & DO_ALTPATH) {
					cmdp = NULL;
					continue;
				}
				break;
			case CMDFUNCTION:
				if (act & DO_NOFUNC) {
					cmdp = NULL;
					continue;
				}
				break;
			case CMDBUILTIN:
				if ((act & DO_ALTBLTIN) || builtinloc >= 0) {
					cmdp = NULL;
					continue;
				}
				break;
			}
			/* if not invalidated by cd, we're done */
			if (cmdp->rehash == 0)
				goto success;
		} while (0);
	}

	/* If %builtin not in path, check for builtin next */
	if ((act & DO_ALTPATH ? !(act & DO_ALTBLTIN) : builtinloc < 0) &&
	    (bltin = find_builtin(name)) != 0)
		goto builtin_success;

	/* We have to search path. */
	prev = -1;		/* where to start */
	if (cmdp) {		/* doing a rehash */
		if (cmdp->cmdtype == CMDBUILTIN)
			prev = builtinloc;
		else
			prev = cmdp->param.index;
	}

	e = ENOENT;
	idx = -1;
loop:
	while ((fullname = padvance(&path, name)) != NULL) {
		stunalloc(fullname);
		idx++;
		if (pathopt) {
			if (prefix("builtin", pathopt)) {
				if ((bltin = find_builtin(name)) == 0)
					goto loop;
				goto builtin_success;
			} else if (prefix("func", pathopt)) {
				/* handled below */
			} else {
				/* ignore unimplemented options */
				goto loop;
			}
		}
		/* if rehash, don't redo absolute path names */
		if (fullname[0] == '/' && idx <= prev) {
			if (idx < prev)
				goto loop;
			TRACE(("searchexec \"%s\": no change\n", name));
			goto success;
		}
		while (stat(fullname, &statb) < 0) {
#ifdef SYSV
			if (errno == EINTR)
				continue;
#endif
			if (errno != ENOENT && errno != ENOTDIR)
				e = errno;
			goto loop;
		}
		e = EACCES;	/* if we fail, this will be the error */
		if (!S_ISREG(statb.st_mode))
			goto loop;
		if (pathopt) {		/* this is a %func directory */
			if (act & DO_NOFUNC)
				goto loop;
			stalloc(strlen(fullname) + 1);
			readcmdfile(fullname);
			if ((cmdp = cmdlookup(name, 0)) == NULL ||
			    cmdp->cmdtype != CMDFUNCTION)
				error("%s not defined in %s", name, fullname);
			stunalloc(fullname);
			goto success;
		}
#ifdef notdef
		/* XXX this code stops root executing stuff, and is buggy
		   if you need a group from the group list. */
		if (statb.st_uid == geteuid()) {
			if ((statb.st_mode & 0100) == 0)
				goto loop;
		} else if (statb.st_gid == getegid()) {
			if ((statb.st_mode & 010) == 0)
				goto loop;
		} else {
			if ((statb.st_mode & 01) == 0)
				goto loop;
		}
#endif
		TRACE(("searchexec \"%s\" returns \"%s\"\n", name, fullname));
		INTOFF;
		if (act & DO_ALTPATH) {
			stalloc(strlen(fullname) + 1);
			cmdp = &loc_cmd;
		} else
			cmdp = cmdlookup(name, 1);
		cmdp->cmdtype = CMDNORMAL;
		cmdp->param.index = idx;
		INTON;
		goto success;
	}

	/* We failed.  If there was an entry for this command, delete it */
	if (cmdp)
		delete_cmd_entry();
	if (act & DO_ERR)
		outfmt(out2, "%s: %s\n", name, errmsg(e, E_EXEC));
	entry->cmdtype = CMDUNKNOWN;
	return;

builtin_success:
	INTOFF;
	if (act & DO_ALTPATH)
		cmdp = &loc_cmd;
	else
		cmdp = cmdlookup(name, 1);
	if (cmdp->cmdtype == CMDFUNCTION)
		/* DO_NOFUNC must have been set */
		cmdp = &loc_cmd;
	cmdp->cmdtype = CMDBUILTIN;
	cmdp->param.bltin = bltin;
	INTON;
success:
	if (cmdp) {
		cmdp->rehash = 0;
		entry->cmdtype = cmdp->cmdtype;
		entry->u = cmdp->param;
	} else
		entry->cmdtype = CMDUNKNOWN;
}



/*
 * Search the table of builtin commands.
 */

int
(*find_builtin(char *name))(int, char **)
{
	const struct builtincmd *bp;

	for (bp = builtincmd ; bp->name ; bp++) {
		if (*bp->name == *name
		    && (*name == '%' || equal(bp->name, name)))
			return bp->builtin;
	}
	return 0;
}

int
(*find_splbltin(char *name))(int, char **)
{
	const struct builtincmd *bp;

	for (bp = splbltincmd ; bp->name ; bp++) {
		if (*bp->name == *name && equal(bp->name, name))
			return bp->builtin;
	}
	return 0;
}

/*
 * At shell startup put special builtins into hash table.
 * ensures they are executed first (see posix).
 * We stop functions being added with the same name
 * (as they are impossible to call)
 */

void
hash_special_builtins(void)
{
	const struct builtincmd *bp;
	struct tblentry *cmdp;

	for (bp = splbltincmd ; bp->name ; bp++) {
		cmdp = cmdlookup(bp->name, 1);
		cmdp->cmdtype = CMDSPLBLTIN;
		cmdp->param.bltin = bp->builtin;
	}
}



/*
 * Called when a cd is done.  Marks all commands so the next time they
 * are executed they will be rehashed.
 */

void
hashcd(void)
{
	struct tblentry **pp;
	struct tblentry *cmdp;

	for (pp = cmdtable ; pp < &cmdtable[CMDTABLESIZE] ; pp++) {
		for (cmdp = *pp ; cmdp ; cmdp = cmdp->next) {
			if (cmdp->cmdtype == CMDNORMAL
			 || (cmdp->cmdtype == CMDBUILTIN && builtinloc >= 0))
				cmdp->rehash = 1;
		}
	}
}



/*
 * Fix command hash table when PATH changed.
 * Called before PATH is changed.  The argument is the new value of PATH;
 * pathval() still returns the old value at this point.
 * Called with interrupts off.
 */

void
changepath(const char *newval)
{
	const char *old, *new;
	int idx;
	int firstchange;
	int bltin;

	old = pathval();
	new = newval;
	firstchange = 9999;	/* assume no change */
	idx = 0;
	bltin = -1;
	for (;;) {
		if (*old != *new) {
			firstchange = idx;
			if ((*old == '\0' && *new == ':')
			 || (*old == ':' && *new == '\0'))
				firstchange++;
			old = new;	/* ignore subsequent differences */
		}
		if (*new == '\0')
			break;
		if (*new == '%' && bltin < 0 && prefix("builtin", new + 1))
			bltin = idx;
		if (*new == ':') {
			idx++;
		}
		new++, old++;
	}
	if (builtinloc < 0 && bltin >= 0)
		builtinloc = bltin;		/* zap builtins */
	if (builtinloc >= 0 && bltin < 0)
		firstchange = 0;
	clearcmdentry(firstchange);
	builtinloc = bltin;
}


/*
 * Clear out command entries.  The argument specifies the first entry in
 * PATH which has changed.
 */

STATIC void
clearcmdentry(int firstchange)
{
	struct tblentry **tblp;
	struct tblentry **pp;
	struct tblentry *cmdp;

	INTOFF;
	for (tblp = cmdtable ; tblp < &cmdtable[CMDTABLESIZE] ; tblp++) {
		pp = tblp;
		while ((cmdp = *pp) != NULL) {
			if ((cmdp->cmdtype == CMDNORMAL &&
			     cmdp->param.index >= firstchange)
			 || (cmdp->cmdtype == CMDBUILTIN &&
			     builtinloc >= firstchange)) {
				*pp = cmdp->next;
				ckfree(cmdp);
			} else {
				pp = &cmdp->next;
			}
		}
	}
	INTON;
}


/*
 * Delete all functions.
 */

#ifdef mkinit
MKINIT void deletefuncs(void);
MKINIT void hash_special_builtins(void);

INIT {
	hash_special_builtins();
}

SHELLPROC {
	deletefuncs();
}
#endif

void
deletefuncs(void)
{
	struct tblentry **tblp;
	struct tblentry **pp;
	struct tblentry *cmdp;

	INTOFF;
	for (tblp = cmdtable ; tblp < &cmdtable[CMDTABLESIZE] ; tblp++) {
		pp = tblp;
		while ((cmdp = *pp) != NULL) {
			if (cmdp->cmdtype == CMDFUNCTION) {
				*pp = cmdp->next;
				freefunc(cmdp->param.func);
				ckfree(cmdp);
			} else {
				pp = &cmdp->next;
			}
		}
	}
	INTON;
}



/*
 * Locate a command in the command hash table.  If "add" is nonzero,
 * add the command to the table if it is not already present.  The
 * variable "lastcmdentry" is set to point to the address of the link
 * pointing to the entry, so that delete_cmd_entry can delete the
 * entry.
 */

struct tblentry **lastcmdentry;


STATIC struct tblentry *
cmdlookup(const char *name, int add)
{
	int hashval;
	const char *p;
	struct tblentry *cmdp;
	struct tblentry **pp;

	p = name;
	hashval = *p << 4;
	while (*p)
		hashval += *p++;
	hashval &= 0x7FFF;
	pp = &cmdtable[hashval % CMDTABLESIZE];
	for (cmdp = *pp ; cmdp ; cmdp = cmdp->next) {
		if (equal(cmdp->cmdname, name))
			break;
		pp = &cmdp->next;
	}
	if (add && cmdp == NULL) {
		INTOFF;
		cmdp = *pp = ckmalloc(sizeof (struct tblentry) - ARB
					+ strlen(name) + 1);
		cmdp->next = NULL;
		cmdp->cmdtype = CMDUNKNOWN;
		cmdp->rehash = 0;
		strcpy(cmdp->cmdname, name);
		INTON;
	}
	lastcmdentry = pp;
	return cmdp;
}

/*
 * Delete the command entry returned on the last lookup.
 */

STATIC void
delete_cmd_entry(void)
{
	struct tblentry *cmdp;

	INTOFF;
	cmdp = *lastcmdentry;
	*lastcmdentry = cmdp->next;
	ckfree(cmdp);
	INTON;
}



#ifdef notdef
void
getcmdentry(char *name, struct cmdentry *entry)
{
	struct tblentry *cmdp = cmdlookup(name, 0);

	if (cmdp) {
		entry->u = cmdp->param;
		entry->cmdtype = cmdp->cmdtype;
	} else {
		entry->cmdtype = CMDUNKNOWN;
		entry->u.index = 0;
	}
}
#endif


/*
 * Add a new command entry, replacing any existing command entry for
 * the same name - except special builtins.
 */

STATIC void
addcmdentry(char *name, struct cmdentry *entry)
{
	struct tblentry *cmdp;

	INTOFF;
	cmdp = cmdlookup(name, 1);
	if (cmdp->cmdtype != CMDSPLBLTIN) {
		if (cmdp->cmdtype == CMDFUNCTION) {
			freefunc(cmdp->param.func);
		}
		cmdp->cmdtype = entry->cmdtype;
		cmdp->param = entry->u;
	}
	INTON;
}


/*
 * Define a shell function.
 */

void
defun(char *name, union node *func)
{
	struct cmdentry entry;

	INTOFF;
	entry.cmdtype = CMDFUNCTION;
	entry.u.func = copyfunc(func);
	addcmdentry(name, &entry);
	INTON;
}


/*
 * Delete a function if it exists.
 */

int
unsetfunc(char *name)
{
	struct tblentry *cmdp;

	if ((cmdp = cmdlookup(name, 0)) != NULL &&
	    cmdp->cmdtype == CMDFUNCTION) {
		freefunc(cmdp->param.func);
		delete_cmd_entry();
	}
	return 0;
}

/*
 * Locate and print what a word is...
 * also used for 'command -[v|V]'
 */

int
typecmd(int argc, char **argv)
{
	struct cmdentry entry;
	struct tblentry *cmdp;
	const char * const *pp;
	struct alias *ap;
	int err = 0;
	char *arg;
	int c;
	int V_flag = 0;
	int v_flag = 0;
	int p_flag = 0;

	while ((c = nextopt("vVp")) != 0) {
		switch (c) {
		case 'v': v_flag = 1; break;
		case 'V': V_flag = 1; break;
		case 'p': p_flag = 1; break;
		}
	}

	if (p_flag && (v_flag || V_flag))
		error("cannot specify -p with -v or -V");

	while ((arg = *argptr++)) {
		if (!v_flag)
			out1str(arg);
		/* First look at the keywords */
		for (pp = parsekwd; *pp; pp++)
			if (**pp == *arg && equal(*pp, arg))
				break;

		if (*pp) {
			if (v_flag)
				err = 1;
			else
				out1str(" is a shell keyword\n");
			continue;
		}

		/* Then look at the aliases */
		if ((ap = lookupalias(arg, 1)) != NULL) {
			if (!v_flag)
				out1fmt(" is an alias for \n");
			out1fmt("%s\n", ap->val);
			continue;
		}

		/* Then check if it is a tracked alias */
		if ((cmdp = cmdlookup(arg, 0)) != NULL) {
			entry.cmdtype = cmdp->cmdtype;
			entry.u = cmdp->param;
		} else {
			/* Finally use brute force */
			find_command(arg, &entry, DO_ABS, pathval());
		}

		switch (entry.cmdtype) {
		case CMDNORMAL: {
			if (strchr(arg, '/') == NULL) {
				const char *path = pathval();
				char *name;
				int j = entry.u.index;
				do {
					name = padvance(&path, arg);
					stunalloc(name);
				} while (--j >= 0);
				if (!v_flag)
					out1fmt(" is%s ",
					    cmdp ? " a tracked alias for" : "");
				out1fmt("%s\n", name);
			} else {
				if (access(arg, X_OK) == 0) {
					if (!v_flag)
						out1fmt(" is ");
					out1fmt("%s\n", arg);
				} else {
					if (!v_flag)
						out1fmt(": %s\n",
						    strerror(errno));
					else
						err = 126;
				}
			}
 			break;
		}
		case CMDFUNCTION:
			if (!v_flag)
				out1str(" is a shell function\n");
			else
				out1fmt("%s\n", arg);
			break;

		case CMDBUILTIN:
			if (!v_flag)
				out1str(" is a shell builtin\n");
			else
				out1fmt("%s\n", arg);
			break;

		case CMDSPLBLTIN:
			if (!v_flag)
				out1str(" is a special shell builtin\n");
			else
				out1fmt("%s\n", arg);
			break;

		default:
			if (!v_flag)
				out1str(": not found\n");
			err = 127;
			break;
		}
	}
	return err;
}
