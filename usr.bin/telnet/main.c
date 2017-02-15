/*	$NetBSD: main.c,v 1.29 2012/03/20 20:34:59 matt Exp $	*/

/*
 * Copyright (c) 1988, 1990, 1993
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
__COPYRIGHT("@(#) Copyright (c) 1988, 1990, 1993\
 The Regents of the University of California.  All rights reserved.");
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)main.c	8.3 (Berkeley) 5/30/95";
#else
__RCSID("$NetBSD: main.c,v 1.29 2012/03/20 20:34:59 matt Exp $");
#endif
#endif /* not lint */

#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>

#include "ring.h"
#include "externs.h"
#include "defines.h"
#ifdef AUTHENTICATION
#include <libtelnet/auth.h>
#endif
#ifdef ENCRYPTION
#include <libtelnet/encrypt.h>
#endif

/* These values need to be the same as defined in libtelnet/kerberos5.c */
/* Either define them in both places, or put in some common header file. */
#define OPTS_FORWARD_CREDS	0x00000002
#define OPTS_FORWARDABLE_CREDS	0x00000001

#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
char *ipsec_policy_in = NULL;
char *ipsec_policy_out = NULL;
#endif

int family = AF_UNSPEC;

int main(int, char *[]);

/*
 * Initialize variables.
 */
void
tninit(void)
{
    init_terminal();

    init_network();

    init_telnet();

    init_sys();

#ifdef TN3270
    init_3270();
#endif
}

	void
usage(void)
{
	fprintf(stderr, "usage: %s %s%s%s%s\n",
	    prompt,
#ifdef	AUTHENTICATION
	    "[-4] [-6] [-8] [-E] [-K] [-L] [-N] [-S tos] [-X atype] [-a] [-c]",
	    "\n\t[-d] [-e char] [-k realm] [-l user] [-f/-F] [-n tracefile] ",
#else
	    "[-4] [-6] [-8] [-E] [-L] [-N] [-S tos] [-a] [-c] [-d] [-e char]",
	    "\n\t[-l user] [-n tracefile] ",
#endif
#ifdef TN3270
# ifdef AUTHENTICATION
	    "[-noasynch] [-noasynctty]\n\t[-noasyncnet] [-r] [-t transcom] ",
# else
	    "[-noasynch] [-noasynctty] [-noasyncnet] [-r]\n\t[-t transcom]",
# endif
#else
	    "[-r] ",
#endif
#ifdef	ENCRYPTION
	    "[-x] "
#endif
#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
	    "\n\t[-P policy] [host-name [port]]"
#else
	    "\n\t[host-name [port]]"
#endif
	);
	exit(1);
}

/*
 * main.  Parse arguments, invoke the protocol or command parser.
 */


int
main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int ch;
	char *user;
#ifdef	FORWARD
	extern int forward_flags;
#endif	/* FORWARD */

	tninit();		/* Clear out things */

	TerminalSaveState();

	if ((prompt = strrchr(argv[0], '/')) != NULL)
		++prompt;
	else
		prompt = argv[0];

	user = NULL;

	rlogin = (strncmp(prompt, "rlog", 4) == 0) ? '~' : _POSIX_VDISABLE;
	autologin = -1;

#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
#define IPSECOPT	"P:"
#else
#define IPSECOPT
#endif
	while ((ch = getopt(argc, argv, "468EKLNS:X:acde:fFk:l:n:rt:x"
			IPSECOPT)) != -1) {
#undef IPSECOPT
		switch(ch) {
		case '4':
			family = AF_INET;
			break;
		case '6':
			family = AF_INET6;
			break;
		case '8':
			eight = 3;	/* binary output and input */
			break;
		case 'E':
			rlogin = escape = _POSIX_VDISABLE;
			break;
		case 'K':
#ifdef	AUTHENTICATION
			autologin = 0;
#endif
			break;
		case 'L':
			eight |= 2;	/* binary output only */
			break;
		case 'N':
			doaddrlookup = 0;
			break;
		case 'S':
		    {
			fprintf(stderr,
			   "%s: Warning: -S ignored, no parsetos() support.\n",
								prompt);
		    }
			break;
		case 'X':
#ifdef	AUTHENTICATION
			auth_disable_name(optarg);
#endif
			break;
		case 'a':
			autologin = 1;
			break;
		case 'c':
			skiprc = 1;
			break;
		case 'd':
			telnet_debug = 1;
			break;
		case 'e':
			set_escape_char(optarg);
			break;
		case 'f':
#if defined(AUTHENTICATION) && defined(KRB5) && defined(FORWARD)
			if (forward_flags & OPTS_FORWARD_CREDS) {
			    fprintf(stderr,
				    "%s: Only one of -f and -F allowed.\n",
				    prompt);
			    usage();
			}
			forward_flags |= OPTS_FORWARD_CREDS;
#else
			fprintf(stderr,
			 "%s: Warning: -f ignored, no Kerberos V5 support.\n",
				prompt);
#endif
			break;
		case 'F':
#if defined(AUTHENTICATION) && defined(KRB5) && defined(FORWARD)
			if (forward_flags & OPTS_FORWARD_CREDS) {
			    fprintf(stderr,
				    "%s: Only one of -f and -F allowed.\n",
				    prompt);
			    usage();
			}
			forward_flags |= OPTS_FORWARD_CREDS;
			forward_flags |= OPTS_FORWARDABLE_CREDS;
#else
			fprintf(stderr,
			 "%s: Warning: -F ignored, no Kerberos V5 support.\n",
				prompt);
#endif
			break;
		case 'k':
			fprintf(stderr,
			   "%s: Warning: -k ignored, no Kerberos V4 support.\n",
								prompt);
			break;
		case 'l':
			if(autologin == 0) {
				autologin = -1;
			}
			user = optarg;
			break;
		case 'n':
#ifdef TN3270
			/* distinguish between "-n oasynch" and "-noasynch" */
			if (argv[optind - 1][0] == '-' && argv[optind - 1][1]
			    == 'n' && argv[optind - 1][2] == 'o') {
				if (!strcmp(optarg, "oasynch")) {
					noasynchtty = 1;
					noasynchnet = 1;
				} else if (!strcmp(optarg, "oasynchtty"))
					noasynchtty = 1;
				else if (!strcmp(optarg, "oasynchnet"))
					noasynchnet = 1;
			} else
#endif	/* defined(TN3270) */
				SetNetTrace(optarg);
			break;
		case 'r':
			rlogin = '~';
			break;
		case 't':
#ifdef TN3270
			(void)strlcpy(tline, optarg, sizeof(tline));
			transcom = tline;
#else
			fprintf(stderr,
			   "%s: Warning: -t ignored, no TN3270 support.\n",
								prompt);
#endif
			break;
		case 'x':
#ifdef	ENCRYPTION
			encrypt_auto(1);
			decrypt_auto(1);
#else	/* ENCRYPTION */
			fprintf(stderr,
			    "%s: Warning: -x ignored, no ENCRYPT support.\n",
								prompt);
#endif	/* ENCRYPTION */
			break;
#if defined(IPSEC) && defined(IPSEC_POLICY_IPSEC)
		case 'P':
			if (!strncmp("in", optarg, 2))
				ipsec_policy_in = strdup(optarg);
			else if (!strncmp("out", optarg, 3)) 
				ipsec_policy_out = strdup(optarg);
			else
				usage();
			break;
#endif
		case '?':
		default:
			usage();
			/* NOTREACHED */
		}
	}

	if (autologin == -1) {		/* esc@magic.fi; force  */
#if defined(AUTHENTICATION)
		autologin = 1;
#endif
#if defined(ENCRYPTION)
		encrypt_auto(1);
		decrypt_auto(1);
#endif
	}

	if (autologin == -1)
		autologin = (rlogin == _POSIX_VDISABLE) ? 0 : 1;

	argc -= optind;
	argv += optind;

	if (argc) {
		static char ml[] = "-l";
		char *args[7];
		char ** volatile argp;	/* avoid longjmp clobbering */

		argp = args;
		if (argc > 2)
			usage();
		*argp++ = prompt;
		if (user) {
			*argp++ = ml;
			*argp++ = user;
		}
		*argp++ = argv[0];		/* host */
		if (argc > 1)
			*argp++ = argv[1];	/* port */
		*argp = 0;

		if (setjmp(toplevel) != 0)
			Exit(0);
		if (tn(argp - args, args) == 1)
			return (0);
		else
			return (1);
	}
	(void)setjmp(toplevel);
	for (;;) {
#ifdef TN3270
		if (shell_active)
			shell_continue();
		else
#endif
			command(1, 0, 0);
	}
}
