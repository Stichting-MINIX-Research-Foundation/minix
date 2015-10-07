/* $NetBSD: verify.c,v 1.15 2010/09/01 17:25:57 agc Exp $ */

/*-
 * Copyright (c) 2009,2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Alistair Crooks (agc@NetBSD.org)
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/* Command line program to perform netpgp operations */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>

#include <getopt.h>
#include <netpgp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/*
 * SHA1 is now looking as though it should not be used.  Let's
 * pre-empt this by specifying SHA256 - gpg interoperates just fine
 * with SHA256 - agc, 20090522
 */
#define DEFAULT_HASH_ALG "SHA256"

static const char *usage =
	"\t--verify [options] files... OR\n"
	"\t--cat [--output=file] [options] files...\n"
	"where options are:\n"
	"\t[--coredumps] AND/OR\n"
	"\t[--homedir=<homedir>] AND/OR\n"
	"\t[--keyring=<keyring>] AND/OR\n"
	"\t[--userid=<userid>] AND/OR\n"
	"\t[--maxmemalloc=<number of bytes>] AND/OR\n"
	"\t[--verbose]\n";

enum optdefs {
	/* commands */
	VERIFY,
	VERIFY_CAT,
	VERSION_CMD,
	HELP_CMD,

	/* options */
	SSHKEYS,
	KEYRING,
	USERID,
	ARMOUR,
	HOMEDIR,
	OUTPUT,
	RESULTS,
	VERBOSE,
	COREDUMPS,
	SSHKEYFILE,
	MAX_MEM_ALLOC,

	/* debug */
	OPS_DEBUG
};

#define EXIT_ERROR	2

static struct option options[] = {
	/* file manipulation commands */
	{"verify",	no_argument,		NULL,	VERIFY},
	{"cat",		no_argument,		NULL,	VERIFY_CAT},
	{"vericat",	no_argument,		NULL,	VERIFY_CAT},
	{"verify-cat",	no_argument,		NULL,	VERIFY_CAT},
	{"verify-show",	no_argument,		NULL,	VERIFY_CAT},
	{"verifyshow",	no_argument,		NULL,	VERIFY_CAT},
	{"help",	no_argument,		NULL,	HELP_CMD},
	{"version",	no_argument,		NULL,	VERSION_CMD},
	/* options */
	{"ssh",		no_argument, 		NULL,	SSHKEYS},
	{"ssh-keys",	no_argument, 		NULL,	SSHKEYS},
	{"sshkeyfile",	required_argument, 	NULL,	SSHKEYFILE},
	{"coredumps",	no_argument, 		NULL,	COREDUMPS},
	{"keyring",	required_argument, 	NULL,	KEYRING},
	{"userid",	required_argument, 	NULL,	USERID},
	{"home",	required_argument, 	NULL,	HOMEDIR},
	{"homedir",	required_argument, 	NULL,	HOMEDIR},
	{"armor",	no_argument,		NULL,	ARMOUR},
	{"armour",	no_argument,		NULL,	ARMOUR},
	{"verbose",	no_argument, 		NULL,	VERBOSE},
	{"output",	required_argument, 	NULL,	OUTPUT},
	{"results",	required_argument, 	NULL,	RESULTS},
	{"maxmemalloc",	required_argument, 	NULL,	MAX_MEM_ALLOC},
	{"max-mem",	required_argument, 	NULL,	MAX_MEM_ALLOC},
	{"max-alloc",	required_argument, 	NULL,	MAX_MEM_ALLOC},
	{ NULL,		0,			NULL,	0},
};

/* gather up program variables into one struct */
typedef struct prog_t {
	char	 keyring[MAXPATHLEN + 1];	/* name of keyring */
	char	*progname;			/* program name */
	char	*output;			/* output file name */
	int	 overwrite;			/* overwrite files? */
	int	 armour;			/* ASCII armor */
	int	 detached;			/* use separate file */
	int	 cmd;				/* netpgp command */
} prog_t;


/* print a usage message */
static void
print_usage(const char *usagemsg, char *progname)
{
	(void) fprintf(stderr,
	"%s\nAll bug reports, praise and chocolate, please, to:\n%s\n",
				netpgp_get_info("version"),
				netpgp_get_info("maintainer"));
	(void) fprintf(stderr, "Usage: %s COMMAND OPTIONS:\n%s %s",
		progname, progname, usagemsg);
}

/* read all of stdin into memory */
static int
stdin_to_mem(netpgp_t *netpgp, char **temp, char **out, unsigned *maxsize)
{
	unsigned	 newsize;
	unsigned	 size;
	char		 buf[BUFSIZ * 8];
	char		*loc;
	int	 	 n;

	*maxsize = (unsigned)atoi(netpgp_getvar(netpgp, "max mem alloc"));
	size = 0;
	*out = *temp = NULL;
	while ((n = read(STDIN_FILENO, buf, sizeof(buf))) > 0) {
		/* round up the allocation */
		newsize = size + ((n / BUFSIZ) + 1) * BUFSIZ;
		if (newsize > *maxsize) {
			(void) fprintf(stderr, "bounds check\n");
			return size;
		}
		loc = realloc(*temp, newsize);
		if (loc == NULL) {
			(void) fprintf(stderr, "short read\n");
			return size;
		}
		*temp = loc;
		(void) memcpy(&(*temp)[size], buf, n);
		size += n;
	}
	if ((*out = calloc(1, *maxsize)) == NULL) {
		(void) fprintf(stderr, "Bad alloc\n");
		return 0;
	}
	return (int)size;
}

/* output the text to stdout */
static int
show_output(char *out, int size, const char *header)
{
	int	cc;
	int	n;

	if (size <= 0) {
		(void) fprintf(stderr, "%s\n", header);
		return 0;
	}
	for (cc = 0 ; cc < size ; cc += n) {
		if ((n = write(STDOUT_FILENO, &out[cc], size - cc)) <= 0) {
			break;
		}
	}
	if (cc < size) {
		(void) fprintf(stderr, "Short write\n");
		return 0;
	}
	return cc == size;
}

/* do a command once for a specified file 'f' */
static int
netpgp_cmd(netpgp_t *netpgp, prog_t *p, char *f)
{
	unsigned	 maxsize;
	char		*out;
	char		*in;
	int		 ret;
	int		 cc;

	if (f == NULL) {
		cc = stdin_to_mem(netpgp, &in, &out, &maxsize);
		ret = netpgp_verify_memory(netpgp, in, cc,
				(p->cmd == VERIFY_CAT) ? out : NULL,
				(p->cmd == VERIFY_CAT) ? maxsize : 0,
				p->armour);
		ret = show_output(out, ret, "Bad memory verification");
		free(in);
		free(out);
		return ret;
	}
	return netpgp_verify_file(netpgp, f,
			(p->cmd == VERIFY) ? NULL :
				(p->output) ? p->output : "-",
			p->armour);
}


int
main(int argc, char **argv)
{
	netpgp_t	netpgp;
	prog_t          p;
	int             optindex;
	int             homeset;
	int             ret;
	int             ch;
	int             i;

	(void) memset(&p, 0x0, sizeof(p));
	(void) memset(&netpgp, 0x0, sizeof(netpgp));
	homeset = 0;
	p.progname = argv[0];
	p.overwrite = 1;
	p.output = NULL;
	if (argc < 2) {
		print_usage(usage, p.progname);
		exit(EXIT_ERROR);
	}
	/* set some defaults */
	netpgp_setvar(&netpgp, "hash", DEFAULT_HASH_ALG);
	/* 4 MiB for a memory file */
	netpgp_setvar(&netpgp, "max mem alloc", "4194304");
	optindex = 0;
	while ((ch = getopt_long(argc, argv, "", options, &optindex)) != -1) {
		switch (options[optindex].val) {
		case COREDUMPS:
			netpgp_setvar(&netpgp, "coredumps", "allowed");
			p.cmd = options[optindex].val;
			break;
		case VERIFY:
		case VERIFY_CAT:
			p.cmd = options[optindex].val;
			break;
		case VERSION_CMD:
			printf(
"%s\nAll bug reports, praise and chocolate, please, to:\n%s\n",
				netpgp_get_info("version"),
				netpgp_get_info("maintainer"));
			exit(EXIT_SUCCESS);
			/* options */
		case SSHKEYS:
			netpgp_setvar(&netpgp, "ssh keys", "1");
			break;
		case KEYRING:
			if (optarg == NULL) {
				(void) fprintf(stderr,
					"No keyring argument provided\n");
				exit(EXIT_ERROR);
			}
			snprintf(p.keyring, sizeof(p.keyring), "%s", optarg);
			break;
		case USERID:
			if (optarg == NULL) {
				(void) fprintf(stderr,
					"No userid argument provided\n");
				exit(EXIT_ERROR);
			}
			netpgp_setvar(&netpgp, "userid", optarg);
			break;
		case ARMOUR:
			p.armour = 1;
			break;
		case VERBOSE:
			netpgp_incvar(&netpgp, "verbose", 1);
			break;
		case HOMEDIR:
			if (optarg == NULL) {
				(void) fprintf(stderr,
				"No home directory argument provided\n");
				exit(EXIT_ERROR);
			}
			netpgp_set_homedir(&netpgp, optarg, NULL, 0);
			homeset = 1;
			break;
		case OUTPUT:
			if (optarg == NULL) {
				(void) fprintf(stderr,
				"No output filename argument provided\n");
				exit(EXIT_ERROR);
			}
			if (p.output) {
				(void) free(p.output);
			}
			p.output = strdup(optarg);
			break;
		case RESULTS:
			if (optarg == NULL) {
				(void) fprintf(stderr,
				"No output filename argument provided\n");
				exit(EXIT_ERROR);
			}
			netpgp_setvar(&netpgp, "results", optarg);
			break;
		case SSHKEYFILE:
			netpgp_setvar(&netpgp, "ssh keys", "1");
			netpgp_setvar(&netpgp, "sshkeyfile", optarg);
			break;
		case MAX_MEM_ALLOC:
			netpgp_setvar(&netpgp, "max mem alloc", optarg);
			break;
		default:
			p.cmd = HELP_CMD;
			break;
		}
	}
	if (!homeset) {
		netpgp_set_homedir(&netpgp, getenv("HOME"),
			netpgp_getvar(&netpgp, "ssh keys") ? "/.ssh" : "/.gnupg", 1);
	}
	/* initialise, and read keys from file */
	if (!netpgp_init(&netpgp)) {
		printf("can't initialise\n");
		exit(EXIT_ERROR);
	}
	/* now do the required action for each of the command line args */
	ret = EXIT_SUCCESS;
	if (optind == argc) {
		if (!netpgp_cmd(&netpgp, &p, NULL)) {
			ret = EXIT_FAILURE;
		}
	} else {
		for (i = optind; i < argc; i++) {
			if (!netpgp_cmd(&netpgp, &p, argv[i])) {
				ret = EXIT_FAILURE;
			}
		}
	}
	netpgp_end(&netpgp);
	exit(ret);
}
