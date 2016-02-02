/*-
 * Copyright (c) 2009 The NetBSD Foundation, Inc.
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
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <mj.h>
#include <netpgp.h>

/*
 * 2048 is the absolute minimum, really - we should really look at
 * bumping this to 4096 or even higher - agc, 20090522
 */
#define DEFAULT_NUMBITS 2048

#define DEFAULT_HASH_ALG "SHA256"

static const char *usage =
	" --help OR\n"
	"\t--export-key [options] OR\n"
	"\t--find-key [options] OR\n"
	"\t--generate-key [options] OR\n"
	"\t--import-key [options] OR\n"
	"\t--list-keys [options] OR\n"
	"\t--list-sigs [options] OR\n"
	"\t--trusted-keys [options] OR\n"
	"\t--get-key keyid [options] OR\n"
	"\t--version\n"
	"where options are:\n"
	"\t[--cipher=<cipher name>] AND/OR\n"
	"\t[--coredumps] AND/OR\n"
	"\t[--hash=<hash alg>] AND/OR\n"
	"\t[--homedir=<homedir>] AND/OR\n"
	"\t[--keyring=<keyring>] AND/OR\n"
	"\t[--userid=<userid>] AND/OR\n"
	"\t[--verbose]\n";

enum optdefs {
	/* commands */
	LIST_KEYS = 260,
	LIST_SIGS,
	FIND_KEY,
	EXPORT_KEY,
	IMPORT_KEY,
	GENERATE_KEY,
	VERSION_CMD,
	HELP_CMD,
	GET_KEY,
	TRUSTED_KEYS,

	/* options */
	SSHKEYS,
	KEYRING,
	USERID,
	HOMEDIR,
	NUMBITS,
	HASH_ALG,
	VERBOSE,
	COREDUMPS,
	PASSWDFD,
	RESULTS,
	SSHKEYFILE,
	CIPHER,
	FORMAT,

	/* debug */
	OPS_DEBUG

};

#define EXIT_ERROR	2

static struct option options[] = {
	/* key-management commands */
	{"list-keys",	no_argument,		NULL,	LIST_KEYS},
	{"list-sigs",	no_argument,		NULL,	LIST_SIGS},
	{"find-key",	optional_argument,	NULL,	FIND_KEY},
	{"export",	no_argument,		NULL,	EXPORT_KEY},
	{"export-key",	no_argument,		NULL,	EXPORT_KEY},
	{"import",	no_argument,		NULL,	IMPORT_KEY},
	{"import-key",	no_argument,		NULL,	IMPORT_KEY},
	{"gen",		optional_argument,	NULL,	GENERATE_KEY},
	{"gen-key",	optional_argument,	NULL,	GENERATE_KEY},
	{"generate",	optional_argument,	NULL,	GENERATE_KEY},
	{"generate-key", optional_argument,	NULL,	GENERATE_KEY},
	{"get-key", 	no_argument,		NULL,	GET_KEY},
	{"trusted-keys",optional_argument,	NULL,	TRUSTED_KEYS},
	{"trusted",	optional_argument,	NULL,	TRUSTED_KEYS},
	/* debugging commands */
	{"help",	no_argument,		NULL,	HELP_CMD},
	{"version",	no_argument,		NULL,	VERSION_CMD},
	{"debug",	required_argument, 	NULL,	OPS_DEBUG},
	/* options */
	{"coredumps",	no_argument, 		NULL,	COREDUMPS},
	{"keyring",	required_argument, 	NULL,	KEYRING},
	{"userid",	required_argument, 	NULL,	USERID},
	{"format",	required_argument, 	NULL,	FORMAT},
	{"hash-alg",	required_argument, 	NULL,	HASH_ALG},
	{"hash",	required_argument, 	NULL,	HASH_ALG},
	{"algorithm",	required_argument, 	NULL,	HASH_ALG},
	{"home",	required_argument, 	NULL,	HOMEDIR},
	{"homedir",	required_argument, 	NULL,	HOMEDIR},
	{"numbits",	required_argument, 	NULL,	NUMBITS},
	{"ssh",		no_argument, 		NULL,	SSHKEYS},
	{"ssh-keys",	no_argument, 		NULL,	SSHKEYS},
	{"sshkeyfile",	required_argument, 	NULL,	SSHKEYFILE},
	{"verbose",	no_argument, 		NULL,	VERBOSE},
	{"pass-fd",	required_argument, 	NULL,	PASSWDFD},
	{"results",	required_argument, 	NULL,	RESULTS},
	{"cipher",	required_argument, 	NULL,	CIPHER},
	{ NULL,		0,			NULL,	0},
};

/* gather up program variables into one struct */
typedef struct prog_t {
	char	 keyring[MAXPATHLEN + 1];	/* name of keyring */
	char	*progname;			/* program name */
	int	 numbits;			/* # of bits */
	int	 cmd;				/* netpgpkeys command */
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

/* match keys, decoding from json if we do find any */
static int
match_keys(netpgp_t *netpgp, FILE *fp, char *f, const int psigs)
{
	char	*json;
	int	 idc;

	if (f == NULL) {
		if (!netpgp_list_keys_json(netpgp, &json, psigs)) {
			return 0;
		}
	} else {
		if (netpgp_match_keys_json(netpgp, &json, f,
				netpgp_getvar(netpgp, "format"), psigs) == 0) {
			return 0;
		}
	}
	idc = netpgp_format_json(fp, json, psigs);
	/* clean up */
	free(json);
	return idc;
}

/* do a command once for a specified file 'f' */
static int
netpgp_cmd(netpgp_t *netpgp, prog_t *p, char *f)
{
	char	*key;
	char	*s;

	switch (p->cmd) {
	case LIST_KEYS:
	case LIST_SIGS:
		return match_keys(netpgp, stdout, f, (p->cmd == LIST_SIGS));
	case FIND_KEY:
		if ((key = f) == NULL) {
			key = netpgp_getvar(netpgp, "userid");
		}
		return netpgp_find_key(netpgp, key);
	case EXPORT_KEY:
		if ((key = f) == NULL) {
			key = netpgp_getvar(netpgp, "userid");
		}
		if (key) {
			if ((s = netpgp_export_key(netpgp, key)) != NULL) {
				printf("%s", s);
				return 1;
			}
		}
		(void) fprintf(stderr, "key '%s' not found\n", f);
		return 0;
	case IMPORT_KEY:
		return netpgp_import_key(netpgp, f);
	case GENERATE_KEY:
		return netpgp_generate_key(netpgp, f, p->numbits);
	case GET_KEY:
		key = netpgp_get_key(netpgp, f, netpgp_getvar(netpgp, "format"));
		if (key) {
			printf("%s", key);
			return 1;
		}
		(void) fprintf(stderr, "key '%s' not found\n", f);
		return 0;
	case TRUSTED_KEYS:
		return netpgp_match_pubkeys(netpgp, f, stdout);
	case HELP_CMD:
	default:
		print_usage(usage, p->progname);
		exit(EXIT_SUCCESS);
	}
}

/* set the option */
static int
setoption(netpgp_t *netpgp, prog_t *p, int val, char *arg, int *homeset)
{
	switch (val) {
	case COREDUMPS:
		netpgp_setvar(netpgp, "coredumps", "allowed");
		break;
	case GENERATE_KEY:
		netpgp_setvar(netpgp, "userid checks", "skip");
		p->cmd = val;
		break;
	case LIST_KEYS:
	case LIST_SIGS:
	case FIND_KEY:
	case EXPORT_KEY:
	case IMPORT_KEY:
	case GET_KEY:
	case TRUSTED_KEYS:
	case HELP_CMD:
		p->cmd = val;
		break;
	case VERSION_CMD:
		printf(
"%s\nAll bug reports, praise and chocolate, please, to:\n%s\n",
			netpgp_get_info("version"),
			netpgp_get_info("maintainer"));
		exit(EXIT_SUCCESS);
		/* options */
	case SSHKEYS:
		netpgp_setvar(netpgp, "ssh keys", "1");
		break;
	case KEYRING:
		if (arg == NULL) {
			(void) fprintf(stderr,
				"No keyring argument provided\n");
			exit(EXIT_ERROR);
		}
		snprintf(p->keyring, sizeof(p->keyring), "%s", arg);
		break;
	case USERID:
		if (optarg == NULL) {
			(void) fprintf(stderr,
				"no userid argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_setvar(netpgp, "userid", arg);
		break;
	case VERBOSE:
		netpgp_incvar(netpgp, "verbose", 1);
		break;
	case HOMEDIR:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"no home directory argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_set_homedir(netpgp, arg, NULL, 0);
		*homeset = 1;
		break;
	case NUMBITS:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"no number of bits argument provided\n");
			exit(EXIT_ERROR);
		}
		p->numbits = atoi(arg);
		break;
	case HASH_ALG:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"No hash algorithm argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_setvar(netpgp, "hash", arg);
		break;
	case PASSWDFD:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"no pass-fd argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_setvar(netpgp, "pass-fd", arg);
		break;
	case RESULTS:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"No output filename argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_setvar(netpgp, "res", arg);
		break;
	case SSHKEYFILE:
		netpgp_setvar(netpgp, "ssh keys", "1");
		netpgp_setvar(netpgp, "sshkeyfile", arg);
		break;
	case FORMAT:
		netpgp_setvar(netpgp, "format", arg);
		break;
	case CIPHER:
		netpgp_setvar(netpgp, "cipher", arg);
		break;
	case OPS_DEBUG:
		netpgp_set_debug(arg);
		break;
	default:
		p->cmd = HELP_CMD;
		break;
	}
	return 1;
}

/* we have -o option=value -- parse, and process */
static int
parse_option(netpgp_t *netpgp, prog_t *p, const char *s, int *homeset)
{
	static regex_t	 opt;
	struct option	*op;
	static int	 compiled;
	regmatch_t	 matches[10];
	char		 option[128];
	char		 value[128];

	if (!compiled) {
		compiled = 1;
		(void) regcomp(&opt, "([^=]{1,128})(=(.*))?", REG_EXTENDED);
	}
	if (regexec(&opt, s, 10, matches, 0) == 0) {
		(void) snprintf(option, sizeof(option), "%.*s",
			(int)(matches[1].rm_eo - matches[1].rm_so), &s[matches[1].rm_so]);
		if (matches[2].rm_so > 0) {
			(void) snprintf(value, sizeof(value), "%.*s",
				(int)(matches[3].rm_eo - matches[3].rm_so), &s[matches[3].rm_so]);
		} else {
			value[0] = 0x0;
		}
		for (op = options ; op->name ; op++) {
			if (strcmp(op->name, option) == 0) {
				return setoption(netpgp, p, op->val, value, homeset);
			}
		}
	}
	return 0;
}

int
main(int argc, char **argv)
{
	struct stat	st;
	netpgp_t	netpgp;
	prog_t          p;
	int             homeset;
	int             optindex;
	int             ret;
	int             ch;
	int             i;

	(void) memset(&p, 0x0, sizeof(p));
	(void) memset(&netpgp, 0x0, sizeof(netpgp));
	homeset = 0;
	p.progname = argv[0];
	p.numbits = DEFAULT_NUMBITS;
	if (argc < 2) {
		print_usage(usage, p.progname);
		exit(EXIT_ERROR);
	}
	/* set some defaults */
	netpgp_setvar(&netpgp, "sshkeydir", "/etc/ssh");
	netpgp_setvar(&netpgp, "res", "<stdout>");
	netpgp_setvar(&netpgp, "hash", DEFAULT_HASH_ALG);
	netpgp_setvar(&netpgp, "format", "human");
	optindex = 0;
	while ((ch = getopt_long(argc, argv, "S:Vglo:s", options, &optindex)) != -1) {
		if (ch >= LIST_KEYS) {
			/* getopt_long returns 0 for long options */
			if (!setoption(&netpgp, &p, options[optindex].val, optarg, &homeset)) {
				(void) fprintf(stderr, "Bad setoption result %d\n", ch);
			}
		} else {
			switch (ch) {
			case 'S':
				netpgp_setvar(&netpgp, "ssh keys", "1");
				netpgp_setvar(&netpgp, "sshkeyfile", optarg);
				break;
			case 'V':
				printf(
	"%s\nAll bug reports, praise and chocolate, please, to:\n%s\n",
					netpgp_get_info("version"),
					netpgp_get_info("maintainer"));
				exit(EXIT_SUCCESS);
			case 'g':
				p.cmd = GENERATE_KEY;
				break;
			case 'l':
				p.cmd = LIST_KEYS;
				break;
			case 'o':
				if (!parse_option(&netpgp, &p, optarg, &homeset)) {
					(void) fprintf(stderr, "Bad parse_option\n");
				}
				break;
			case 's':
				p.cmd = LIST_SIGS;
				break;
			default:
				p.cmd = HELP_CMD;
				break;
			}
		}
	}
	if (!homeset) {
		netpgp_set_homedir(&netpgp, getenv("HOME"),
			netpgp_getvar(&netpgp, "ssh keys") ? "/.ssh" : "/.gnupg", 1);
	}
	/* initialise, and read keys from file */
	if (!netpgp_init(&netpgp)) {
		if (stat(netpgp_getvar(&netpgp, "homedir"), &st) < 0) {
			(void) mkdir(netpgp_getvar(&netpgp, "homedir"), 0700);
		}
		if (stat(netpgp_getvar(&netpgp, "homedir"), &st) < 0) {
			(void) fprintf(stderr, "can't create home directory '%s'\n",
				netpgp_getvar(&netpgp, "homedir"));
			exit(EXIT_ERROR);
		}
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
