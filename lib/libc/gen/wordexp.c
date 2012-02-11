/*	$NetBSD: wordexp.c,v 1.3 2009/02/12 04:10:52 lukem Exp $	*/

/*-
 * Copyright (c) 2002 Tim J. Robbins.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "namespace.h"
#include <sys/cdefs.h>
#include <sys/types.h>
#include <assert.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wordexp.h>

#if defined(LIBC_SCCS) && !defined(lint)
#if 0
__FBSDID("$FreeBSD: /repoman/r/ncvs/src/lib/libc/gen/wordexp.c,v 1.5 2004/04/09 11:32:32 tjr Exp $");
#else
__RCSID("$NetBSD: wordexp.c,v 1.3 2009/02/12 04:10:52 lukem Exp $");
#endif
#endif /* LIBC_SCCS and not lint */

static int	we_askshell(const char *, wordexp_t *, int);
static int	we_check(const char *, int);

/*
 * wordexp --
 *	Perform shell word expansion on `words' and place the resulting list
 *	of words in `we'. See wordexp(3).
 *
 */
int
wordexp(const char * __restrict words, wordexp_t * __restrict we, int flags)
{
	int error;

	_DIAGASSERT(we != NULL);
	_DIAGASSERT(words != NULL);
	if (flags & WRDE_REUSE)
		wordfree(we);
	if ((flags & WRDE_APPEND) == 0) {
		we->we_wordc = 0;
		we->we_wordv = NULL;
		we->we_strings = NULL;
		we->we_nbytes = 0;
	}
	if ((error = we_check(words, flags)) != 0) {
		wordfree(we);
		return (error);
	}
	if ((error = we_askshell(words, we, flags)) != 0) {
		wordfree(we);
		return (error);
	}
	return (0);
}

/*
 * we_askshell --
 *	Use the `wordexp' /bin/sh builtin function to do most of the work
 *	in expanding the word string. This function is complicated by
 *	memory management.
 */
static int
we_askshell(const char *words, wordexp_t *we, int flags)
{
	int pdes[2];			/* Pipe to child */
	size_t nwords, nbytes;		/* Number of words, bytes from child */
	int i;				/* Handy integer */
	unsigned int ui;		/* For array iteration */
	size_t sofs;			/* Offset into we->we_strings */
	size_t vofs;			/* Offset into we->we_wordv */
	pid_t pid;			/* Process ID of child */
	int status;			/* Child exit status */
	const char *ifs;		/* IFS env. var. */
	char *np, *p;			/* Handy pointers */
	char *nstrings;			/* Temporary for realloc() */
	char **nwv;			/* Temporary for realloc() */
	FILE *fp;			/* Stream to read pipe */
	extern char **environ;
	char *cmd;

	if ((ifs = getenv("IFS")) == NULL)
		ifs = " \t\n";
	if (asprintf(&cmd, "wordexp%c%s\n", *ifs, words) < 0)
		return (WRDE_NOSPACE);
	if (pipe(pdes) < 0) {
		free(cmd);
		return (WRDE_ERRNO);
	}
	if ((fp = fdopen(pdes[0], "r")) == NULL) {
		free(cmd);
		return (WRDE_ERRNO);
	}
	if ((pid = fork()) < 0) {
		free(cmd);
		fclose(fp);
		close(pdes[1]);
		return (WRDE_ERRNO);
	}
	else if (pid == 0) {
		/*
		 * We are the child; just get /bin/sh to run the wordexp
		 * builtin on `words'.
		 */
		int devnull;

		close(pdes[0]);
		if (pdes[1] != STDOUT_FILENO) {
			if (dup2(pdes[1], STDOUT_FILENO) < 0)
				_exit(1);
			close(pdes[1]);
		}
		if ((flags & WRDE_SHOWERR) == 0) {
			if ((devnull = open(_PATH_DEVNULL, O_RDWR, 0666)) < 0)
				_exit(1);
			if (dup2(devnull, STDERR_FILENO) < 0)
				_exit(1);
			close(devnull);
		}
		execle(_PATH_BSHELL, "sh", flags & WRDE_UNDEF ? "-u" : "+u",
		    "-c", cmd, (char *)NULL, environ);
		_exit(1);
	}

	/*
	 * We are the parent; read the output of the shell wordexp function,
	 * which is a decimal word count, an null, a decimal byte count,
	 * (not including terminating null bytes), a null and then followed
	 * by the expanded words separated by nulls.
	 */
	free(cmd);
	close(pdes[1]);
	/* read the word count */	
	nwords = 0;
	while ((i = getc(fp)) != EOF) {
		if (i == '\0')
			break;
		nwords *= 10;
		nwords += (i - '0');
	}
	/* read the byte count */
	nbytes = 0;
	while ((i = getc(fp)) != EOF) {
		if (i == '\0')
			break;
		nbytes *= 10;
		nbytes += (i - '0');
	}
	if (i == EOF) {
		fclose(fp);
		waitpid(pid, &status, 0);
		return (flags & WRDE_UNDEF ? WRDE_BADVAL : WRDE_SYNTAX);
	}
	nbytes += nwords;

	/*
	 * Allocate or reallocate (when flags & WRDE_APPEND) the word vector
	 * and string storage buffers for the expanded words we're about to
	 * read from the child.
	 */
	sofs = we->we_nbytes;
	vofs = we->we_wordc;
	if ((flags & (WRDE_DOOFFS|WRDE_APPEND)) == (WRDE_DOOFFS|WRDE_APPEND))
		vofs += we->we_offs;
	we->we_wordc += nwords;
	we->we_nbytes += nbytes;
	if ((nwv = realloc(we->we_wordv, (we->we_wordc + 1 +
	    (flags & WRDE_DOOFFS ?  we->we_offs : 0)) *
	    sizeof(char *))) == NULL) {
		fclose(fp);
		waitpid(pid, &status, 0);
		return (WRDE_NOSPACE);
	}
	we->we_wordv = nwv;
	if ((nstrings = realloc(we->we_strings, we->we_nbytes)) == NULL) {
		fclose(fp);
		waitpid(pid, &status, 0);
		return (WRDE_NOSPACE);
	}
	for (ui = 0; ui < vofs; ui++)
		if (we->we_wordv[ui] != NULL)
			we->we_wordv[ui] += nstrings - we->we_strings;
	we->we_strings = nstrings;

	if (fread(we->we_strings + sofs, sizeof(char), nbytes, fp) != nbytes) {
		fclose(fp);
		waitpid(pid, &status, 0);
		return (flags & WRDE_UNDEF ? WRDE_BADVAL : WRDE_SYNTAX);
	}

	if (waitpid(pid, &status, 0) < 0 || !WIFEXITED(status) ||
	    WEXITSTATUS(status) != 0) {
		fclose(fp);
		return (flags & WRDE_UNDEF ? WRDE_BADVAL : WRDE_SYNTAX);
	}
	fclose(fp);

	/*
	 * Break the null-terminated expanded word strings out into
	 * the vector.
	 */
	if (vofs == 0 && flags & WRDE_DOOFFS)
		while (vofs < we->we_offs)
			we->we_wordv[vofs++] = NULL;
	p = we->we_strings + sofs;
	while (nwords-- != 0) {
		we->we_wordv[vofs++] = p;
		if ((np = memchr(p, '\0', nbytes)) == NULL)
			return (WRDE_NOSPACE);	/* XXX */
		nbytes -= np - p + 1;
		p = np + 1;
	}
	we->we_wordv[vofs] = NULL;

	return (0);
}

/*
 * we_check --
 *	Check that the string contains none of the following unquoted
 *	special characters: <newline> |&;<>(){}
 *	or command substitutions when WRDE_NOCMD is set in flags.
 */
static int
we_check(const char *words, int flags)
{
	char c;
	int dquote, level, quote, squote;

	quote = squote = dquote = 0;
	while ((c = *words++) != '\0') {
		switch (c) {
		case '\\':
			quote ^= 1;
			continue;
		case '\'':
			if (quote + dquote == 0)
				squote ^= 1;
			break;
		case '"':
			if (quote + squote == 0)
				dquote ^= 1;
			break;
		case '`':
			if (quote + squote == 0 && flags & WRDE_NOCMD)
				return (WRDE_CMDSUB);
			while ((c = *words++) != '\0' && c != '`')
				if (c == '\\' && (c = *words++) == '\0')
					break;
			if (c == '\0')
				return (WRDE_SYNTAX);
			break;
		case '|': case '&': case ';': case '<': case '>':
		case '{': case '}': case '(': case ')': case '\n':
			if (quote + squote + dquote == 0)
				return (WRDE_BADCHAR);
			break;
		case '$':
			if ((c = *words++) == '\0')
				break;
			else if (quote + squote == 0 && c == '(') {
				if (flags & WRDE_NOCMD && *words != '(')
					return (WRDE_CMDSUB);
				level = 1;
				while ((c = *words++) != '\0') {
					if (c == '\\') {
						if ((c = *words++) == '\0')
							break;
					} else if (c == '(')
						level++;
					else if (c == ')' && --level == 0)
						break;
				}
				if (c == '\0' || level != 0)
					return (WRDE_SYNTAX);
			} else if (quote + squote == 0 && c == '{') {
				level = 1;
				while ((c = *words++) != '\0') {
					if (c == '\\') {
						if ((c = *words++) == '\0')
							break;
					} else if (c == '{')
						level++;
					else if (c == '}' && --level == 0)
						break;
				}
				if (c == '\0' || level != 0)
					return (WRDE_SYNTAX);
			} else
				c = *--words;
			break;
		default:
			break;
		}
		quote = 0;
	}
	if (quote + squote + dquote != 0)
		return (WRDE_SYNTAX);

	return (0);
}

/*
 * wordfree --
 *	Free the result of wordexp(). See wordexp(3).
 *
 */
void
wordfree(wordexp_t *we)
{
	_DIAGASSERT(we != NULL);
	free(we->we_wordv);
	free(we->we_strings);
	we->we_wordv = NULL;
	we->we_strings = NULL;
	we->we_nbytes = 0;
	we->we_wordc = 0;
}
