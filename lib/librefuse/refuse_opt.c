/* 	$NetBSD: refuse_opt.c,v 1.14 2009/01/19 09:56:06 lukem Exp $	*/

/*-
 * Copyright (c) 2007 Juan Romero Pardines.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * TODO:
 * 	* -oblah,foo... works, but the options are not enabled.
 * 	* -ofoo=%s (accepts a string) or -ofoo=%u (int) is not
 * 	  supported for now.
 * 	* void *data: how is it used? I think it's used to enable
 * 	  options or pass values for the matching options.
 */

#include <sys/types.h>

#include <err.h>
#include <fuse.h>
#include <fuse_opt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FUSE_OPT_DEBUG
#define DPRINTF(x)	do { printf x; } while ( /* CONSTCOND */ 0)
#else
#define DPRINTF(x)
#endif

enum {
	KEY_HELP,
	KEY_VERBOSE,
	KEY_VERSION
};

struct fuse_opt_option {
	const struct fuse_opt *fop;
	char *option;
	int key;
	void *data;
};

static int fuse_opt_popt(struct fuse_opt_option *, const struct fuse_opt *);

/* 
 * Public API.
 *
 * The following functions always return 0:
 *
 * int	fuse_opt_add_opt(char **, const char *);
 *
 * We implement the next ones:
 *
 * int	fuse_opt_add_arg(struct fuse_args *, const char *);
 * void	fuse_opt_free_args(struct fuse_args *);
 * int	fuse_opt_insert_arg(struct fuse_args *, const char *);
 * int	fuse_opt_match(const struct fuse_opt *, const char *);
 * int	fuse_opt_parse(struct fuse_args *, void *,
 * 		       const struct fuse_opt *, fuse_opt_proc_t);
 *
 */

/* ARGSUSED */
int
fuse_opt_add_arg(struct fuse_args *args, const char *arg)
{
	struct fuse_args	*ap;

	if (args->allocated == 0) {
		ap = fuse_opt_deep_copy_args(args->argc, args->argv);
		args->argv = ap->argv;
		args->argc = ap->argc;
		args->allocated = ap->allocated;
		(void) free(ap);
	} else if (args->allocated == args->argc) {
		void *a;
		int na = args->allocated + 10;

		if ((a = realloc(args->argv, na * sizeof(*args->argv))) == NULL)
			return -1;

		args->argv = a;
		args->allocated = na;
	}
	DPRINTF(("%s: arguments passed: [arg:%s]\n", __func__, arg));
	if ((args->argv[args->argc++] = strdup(arg)) == NULL)
		err(1, "fuse_opt_add_arg");
	args->argv[args->argc] = NULL;
        return 0;
}

struct fuse_args *
fuse_opt_deep_copy_args(int argc, char **argv)
{
	struct fuse_args	*ap;
	int			 i;

	if ((ap = malloc(sizeof(*ap))) == NULL)
		err(1, "_fuse_deep_copy_args");
	/* deep copy args structure into channel args */
	ap->allocated = ((argc / 10) + 1) * 10;

	if ((ap->argv = calloc((size_t)ap->allocated,
	    sizeof(*ap->argv))) == NULL)
		err(1, "_fuse_deep_copy_args");

	for (i = 0; i < argc; i++) {
		if ((ap->argv[i] = strdup(argv[i])) == NULL)
			err(1, "_fuse_deep_copy_args");
	}
	ap->argv[ap->argc = i] = NULL;
	return ap;
}

void
fuse_opt_free_args(struct fuse_args *ap)
{
	int	i;

	for (i = 0; i < ap->argc; i++) {
		free(ap->argv[i]);
	}
	free(ap->argv);
	ap->argv = NULL;
	ap->allocated = ap->argc = 0;
}

/* ARGSUSED */
int
fuse_opt_insert_arg(struct fuse_args *args, int pos, const char *arg)
{
	int	i;
	int	na;
	void   *a;

	DPRINTF(("%s: arguments passed: [pos=%d] [arg=%s]\n",
	    __func__, pos, arg));
	if (args->argv == NULL) {
		na = 10;
		a = malloc(na * sizeof(*args->argv));
	} else {
		na = args->allocated + 10;
		a = realloc(args->argv, na * sizeof(*args->argv));
	}
	if (a == NULL) {
		warn("fuse_opt_insert_arg");
		return -1;
	}
	args->argv = a;
	args->allocated = na;

	for (i = args->argc++; i > pos; --i) {
		args->argv[i] = args->argv[i - 1];
	}
	if ((args->argv[pos] = strdup(arg)) == NULL)
		err(1, "fuse_opt_insert_arg");
	args->argv[args->argc] = NULL;
	return 0;
}

/* ARGSUSED */
int fuse_opt_add_opt(char **opts, const char *opt)
{
	DPRINTF(("%s: arguments passed: [opts=%s] [opt=%s]\n",
	    __func__, *opts, opt));
	return 0;
}

/*
 * Returns 0 if opt was matched with any option from opts,
 * otherwise returns 1.
 */
int
fuse_opt_match(const struct fuse_opt *opts, const char *opt)
{
	while (opts++) {
		if (strcmp(opt, opts->templ) == 0)
			return 0;
	}

	return 1;
}

/*
 * Returns 0 if foo->option was matched with any option from opts,
 * and sets the following on match:
 *
 * 	* foo->key is set to the foo->fop->value if offset == -1.
 * 	* foo->fop points to the matched struct opts.
 *
 * otherwise returns 1.
 */
static int
fuse_opt_popt(struct fuse_opt_option *foo, const struct fuse_opt *opts)
{
	int i, found = 0;
	char *match;
	
	if (!foo->option) {
		(void)fprintf(stderr, "fuse: missing argument after -o\n");
		return 1;
	}
	/* 
	 * iterate over argv and opts to see
	 * if there's a match with any template.
	 */
	for (match = strtok(foo->option, ",");
	     match; match = strtok(NULL, ",")) {

		DPRINTF(("%s: specified option='%s'\n", __func__, match));
		found = 0;

		for (i = 0; opts && opts->templ; opts++, i++) {

			DPRINTF(("%s: opts->templ='%s' opts->offset=%d "
			    "opts->value=%d\n", __func__, opts->templ,
			    opts->offset, opts->value));

			/* option is ok */
			if (strcmp(match, opts->templ) == 0) {
				DPRINTF(("%s: option matched='%s'\n",
				    __func__, match));
				found++;
				/*
				 * our fop pointer now points 
				 * to the matched struct opts.
				 */
				foo->fop = opts;
				/* 
				 * assign default key value, necessary for
				 * KEY_HELP, KEY_VERSION and KEY_VERBOSE.
				 */
				if (foo->fop->offset == -1)
					foo->key = foo->fop->value;
				/* reset counter */
				opts -= i;
				break;
			}
		}
		/* invalid option */
		if (!found) {
			(void)fprintf(stderr, "fuse: '%s' is not a "
			    "valid option\n", match);
			return 1;
		}
	}

	return 0;
}

/* ARGSUSED1 */
int
fuse_opt_parse(struct fuse_args *args, void *data,
        const struct fuse_opt *opts, fuse_opt_proc_t proc)
{
	struct fuse_opt_option foo;
	char *buf;
	int i, rv = 0;

	memset(&foo, '\0', sizeof(foo));

	if (!args || !args->argv || !args->argc || !proc)
		return 0;

	if (args->argc == 1)
		return proc(foo.data, *args->argv, FUSE_OPT_KEY_OPT, args);

	/* the real loop to process the arguments */
	for (i = 1; i < args->argc; i++) {

		/* assign current argv string */
		foo.option = buf = args->argv[i];

		/* argvn != -foo... */
		if (buf[0] != '-') {

			foo.key = FUSE_OPT_KEY_NONOPT;
			rv = proc(foo.data, foo.option, foo.key, args);
			if (rv != 0)
				break;

		/* -o was specified... */
		} else if (buf[0] == '-' && buf[1] == 'o') {

			/* -oblah,foo... */
			if (buf[2]) {
				/* skip -o */
				foo.option = args->argv[i] + 2;
			/* -o blah,foo... */
			} else {
				/* 
			 	 * skip current argv and pass to the
			 	 * next one to parse the options.
				 */
				++i;
				foo.option = args->argv[i];
			}

			rv = fuse_opt_popt(&foo, opts);
			if (rv != 0)
				break;

		/* help/version/verbose argument */
		} else if (buf[0] == '-' && buf[1] != 'o') {
			/* 
			 * check if the argument matches
			 * with any template in opts.
			 */
			rv = fuse_opt_popt(&foo, opts);
			if (rv != 0) {
				break;
			} else {
				DPRINTF(("%s: foo.fop->templ='%s' "
			    	    "foo.fop->offset: %d "
			    	    "foo.fop->value: %d\n",
			    	    __func__, foo.fop->templ,
			    	    foo.fop->offset, foo.fop->value));

				/* argument needs to be discarded */
				if (foo.key == FUSE_OPT_KEY_DISCARD) {
					rv = 1;
					break;
				}

				/* process help/version argument */
				if (foo.key != KEY_VERBOSE &&
				    foo.key != FUSE_OPT_KEY_KEEP) {
					rv = proc(foo.data, foo.option,
				    		  foo.key, args);
					break;
				} else {
					/* process verbose argument */
					rv = proc(foo.data, foo.option,
						       foo.key, args);
					if (rv != 0)
						break;
				}
			}
		/* unknown option, how could that happen? */
		} else {
			DPRINTF(("%s: unknown option\n", __func__));
			rv = 1;
			break;
		}
	}

	return rv;
}
