/*	$NetBSD: passwd.c,v 1.30 2009/04/17 20:25:08 dyoung Exp $	*/

/*
 * Copyright (c) 1988, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1993, 1994\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "from: @(#)passwd.c    8.3 (Berkeley) 4/2/94";
#else
__RCSID("$NetBSD: passwd.c,v 1.30 2009/04/17 20:25:08 dyoung Exp $");
#endif
#endif /* not lint */

#include <assert.h>
#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>

#include "extern.h"

#ifdef USE_PAM

static void global_usage(const char *);

static const struct pw_module_s {
	const char *argv0;
	const char *dbname;
	char compat_opt;
	void (*pw_usage)(const char *);
	void (*pw_process)(const char *, int, char **);
} pw_modules[] = {
	/* "files" -- local password database */
	{ NULL, "files", 'l', pwlocal_usage, pwlocal_process },
#ifdef YP
	/* "nis" -- YP/NIS password database */
	{ NULL, "nis", 'y', pwyp_usage, pwyp_process },
	{ "yppasswd", NULL, 0, pwyp_argv0_usage, pwyp_process },
#endif
#ifdef KERBEROS5
	/* "krb5" -- Kerberos 5 password database */
	{ NULL, "krb5", 'k', pwkrb5_usage, pwkrb5_process },
	{ "kpasswd", NULL, 0, pwkrb5_argv0_usage, pwkrb5_process },
#endif
	/* default -- use whatever PAM decides */
	{ NULL, NULL, 0, NULL, pwpam_process },

	{ NULL, NULL, 0, NULL, NULL }
};

static const struct pw_module_s *personality;

static void
global_usage(const char *prefix)
{
	const struct pw_module_s *pwm;

	(void) fprintf(stderr, "%s %s [user]\n", prefix, getprogname());
	for (pwm = pw_modules; pwm->pw_process != NULL; pwm++) {
		if (pwm->argv0 == NULL && pwm->pw_usage != NULL)
			(*pwm->pw_usage)("      ");
	}
}

void
usage(void)
{

	if (personality != NULL && personality->pw_usage != NULL)
		(*personality->pw_usage)("usage:");
	else
		global_usage("usage:");
	exit(1);
}

int
main(int argc, char **argv)
{
	const struct pw_module_s *pwm;
	const char *username;
	int ch, i;
	char opts[16];

	/* Build opts string from module compat_opts */
	i = 0;
	opts[i++] = 'd';
	opts[i++] = ':';
	for (pwm = pw_modules; pwm->pw_process != NULL; pwm++) {
		if (pwm->compat_opt != 0)
			opts[i++] = pwm->compat_opt;
	}
	opts[i++] = '\0';

	/* First, look for personality based on argv[0]. */
	for (pwm = pw_modules; pwm->pw_process != NULL; pwm++) {
		if (pwm->argv0 != NULL &&
		    strcmp(pwm->argv0, getprogname()) == 0)
			goto got_personality;
	}

	/* Try based on compat_opt or -d. */
	for (ch = 0, pwm = pw_modules; pwm->pw_process != NULL; pwm++) {
		if (pwm->argv0 == NULL && pwm->dbname == NULL &&
		    pwm->compat_opt == 0) {
			/*
			 * We have reached the default personality case.
			 * Make sure the user didn't provide a bogus
			 * personality name.
			 */
			if (ch == 'd')
				usage();
			break;
		}

		ch = getopt(argc, argv, opts);
		if (ch == '?')
			usage();

		if (ch == 'd' && pwm->dbname != NULL &&
		    strcmp(pwm->dbname, optarg) == 0) {
			/*
			 * "passwd -d dbname" matches; this is our
			 * chosen personality.
			 */
			break;
		}

		if (pwm->compat_opt != 0 && ch == pwm->compat_opt) {
			/*
			 * Legacy "passwd -l" or similar matches; this
			 * is our chosen personality.
			 */
			break;
		}

		/* Reset getopt() and go around again. */
		optind = 1;
		optreset = 1;
	}

 got_personality:
	personality = pwm;

	/*
	 * At this point, optind should be either 1 ("passwd"),
	 * 2 ("passwd -l"), or 3 ("passwd -d files").  Consume
	 * these arguments and reset getopt() for the modules to use.
	 */
	assert(optind >= 1 && optind <= 3);
	argc -= optind;
	argv += optind;
	optind = 0;
	optreset = 1;

	username = getlogin();
	if (username == NULL)
		errx(1, "who are you ??");

	(*personality->pw_process)(username, argc, argv);
	return 0;
}

#else /* ! USE_PAM */

static struct pw_module_s {
	const char *argv0;
	const char *args;
	const char *usage;
	int (*pw_init) __P((const char *));
	int (*pw_arg) __P((char, const char *));
	int (*pw_arg_end) __P((void));
	void (*pw_end) __P((void));

	int (*pw_chpw) __P((const char*));
	int invalid;
#define	INIT_INVALID 1
#define ARG_INVALID 2
	int use_class;
} pw_modules[] = {
#ifdef KERBEROS5
	{ NULL, "5ku:", "[-5] [-k] [-u principal]",
	    krb5_init, krb5_arg, krb5_arg_end, krb5_end, krb5_chpw, 0, 0 },
	{ "kpasswd", "5ku:", "[-5] [-k] [-u principal]",
	    krb5_init, krb5_arg, krb5_arg_end, krb5_end, krb5_chpw, 0, 0 },
#endif
#ifdef YP
	{ NULL, "y", "[-y]",
	    yp_init, yp_arg, yp_arg_end, yp_end, yp_chpw, 0, 0 },
	{ "yppasswd", "", "[-y]",
	    yp_init, yp_arg, yp_arg_end, yp_end, yp_chpw, 0, 0 },
#endif
	/* local */
	{ NULL, "l", "[-l]",
	    local_init, local_arg, local_arg_end, local_end, local_chpw, 0, 0 },

	/* terminator */
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};
 
static void
usage(void)
{
	int i;

	fprintf(stderr, "usage:\n");
	for (i = 0; pw_modules[i].pw_init != NULL; i++)
		if (! (pw_modules[i].invalid & INIT_INVALID))
			fprintf(stderr, "\t%s %s [user]\n", getprogname(),
			    pw_modules[i].usage);
	exit(1);
}

int
main(int argc, char **argv)
{
	int ch;
	char *username;
	char optstring[64];  /* if we ever get more than 64 args, shoot me. */
	const char *curopt, *oopt;
	int i, j;
	int valid;
	int use_always;

	/* allow passwd modules to do argv[0] specific processing */
	use_always = 0;
	valid = 0;
	for (i = 0; pw_modules[i].pw_init != NULL; i++) {
		pw_modules[i].invalid = 0;
		if (pw_modules[i].argv0) {
			/*
			 * If we have a module that matches this progname, be
			 * sure that no modules but those that match this
			 * progname can be used.  If we have a module that
			 * matches against a particular progname, but does NOT
			 * match this one, don't use that module.
			 */
			if ((strcmp(getprogname(), pw_modules[i].argv0) == 0) &&
			    use_always == 0) {
				for (j = 0; j < i; j++) {
					pw_modules[j].invalid |= INIT_INVALID;
					(*pw_modules[j].pw_end)();
				}
				use_always = 1;
			} else if (use_always == 0)
				pw_modules[i].invalid |= INIT_INVALID;
		} else if (use_always)
			pw_modules[i].invalid |= INIT_INVALID;

		if (pw_modules[i].invalid)
			continue;

		pw_modules[i].invalid |=
		    (*pw_modules[i].pw_init)(getprogname()) ?
		    /* zero on success, non-zero on error */
		    INIT_INVALID : 0;

		if (! pw_modules[i].invalid)
			valid = 1;
	}

	if (valid == 0)
		errx(1, "Can't change password.");

	/* Build the option string from the individual modules' option
	 * strings.  Note that two modules can share a single option
	 * letter. */
	optstring[0] = '\0';
	j = 0;
	for (i = 0; pw_modules[i].pw_init != NULL; i++) {
		if (pw_modules[i].invalid)
			continue;

		curopt = pw_modules[i].args;
		while (*curopt != '\0') {
			if ((oopt = strchr(optstring, *curopt)) == NULL) {
				optstring[j++] = *curopt;
				if (curopt[1] == ':') {
					curopt++;
					optstring[j++] = *curopt;
				}
				optstring[j] = '\0';
			} else if ((oopt[1] == ':' && curopt[1] != ':') ||
			    (oopt[1] != ':' && curopt[1] == ':')) {
				errx(1, "NetBSD ERROR!  Different password "
				    "modules have two different ideas about "
				    "%c argument format.", curopt[0]);
			}
			curopt++;
		}
	}

	while ((ch = getopt(argc, argv, optstring)) != -1)
	{
		valid = 0;
		for (i = 0; pw_modules[i].pw_init != NULL; i++) {
			if (pw_modules[i].invalid)
				continue;
			if ((oopt = strchr(pw_modules[i].args, ch)) != NULL) {
				j = (oopt[1] == ':') ?
				    ! (*pw_modules[i].pw_arg)(ch, optarg) :
				    ! (*pw_modules[i].pw_arg)(ch, NULL);
				if (j != 0)
					pw_modules[i].invalid |= ARG_INVALID;
				if (pw_modules[i].invalid)
					(*pw_modules[i].pw_end)();
			} else {
				/* arg doesn't match this module */
				pw_modules[i].invalid |= ARG_INVALID;
				(*pw_modules[i].pw_end)();
			}
			if (! pw_modules[i].invalid)
				valid = 1;
		}
		if (! valid) {
			usage();
			exit(1);
		}
	}

	/* select which module to use to actually change the password. */
	use_always = 0;
	valid = 0;
	for (i = 0; pw_modules[i].pw_init != NULL; i++)
		if (! pw_modules[i].invalid) {
			pw_modules[i].use_class = (*pw_modules[i].pw_arg_end)();
			if (pw_modules[i].use_class != PW_DONT_USE)
				valid = 1;
			if (pw_modules[i].use_class == PW_USE_FORCE)
				use_always = 1;
		}


	if (! valid)
		/* hang the DJ */
		errx(1, "No valid password module specified.");

	argc -= optind;
	argv += optind;

	username = getlogin();
	if (username == NULL)
		errx(1, "who are you ??");
	
	switch(argc) {
	case 0:
		break;
	case 1:
		username = argv[0];
		break;
	default:
		usage();
		exit(1);
	}

	/* allow for fallback to other chpw() methods. */
	for (i = 0; pw_modules[i].pw_init != NULL; i++) {
		if (pw_modules[i].invalid)
			continue;
		if ((use_always && pw_modules[i].use_class == PW_USE_FORCE) ||
		    (!use_always && pw_modules[i].use_class == PW_USE)) {
			valid = (*pw_modules[i].pw_chpw)(username);
			(*pw_modules[i].pw_end)();
			if (valid >= 0)
				exit(valid);
			/* return value < 0 indicates continuation. */
		}
	}
	exit(1);
}

#endif /* USE_PAM */
