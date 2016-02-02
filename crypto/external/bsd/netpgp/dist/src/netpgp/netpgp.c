/* $NetBSD: netpgp.c,v 1.17 2010/11/29 04:20:12 agc Exp $ */

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
	" --help OR\n"
	"\t--encrypt [--output=file] [options] files... OR\n"
	"\t--decrypt [--output=file] [options] files... OR\n\n"
	"\t--sign [--armor] [--detach] [--hash=alg] [--output=file]\n"
		"\t\t[options] files... OR\n"
	"\t--verify [options] files... OR\n"
	"\t--cat [--output=file] [options] files... OR\n"
	"\t--clearsign [--output=file] [options] files... OR\n"
	"\t--list-packets [options] OR\n"
	"\t--version\n"
	"where options are:\n"
	"\t[--cipher=<ciphername>] AND/OR\n"
	"\t[--coredumps] AND/OR\n"
	"\t[--homedir=<homedir>] AND/OR\n"
	"\t[--keyring=<keyring>] AND/OR\n"
	"\t[--numtries=<attempts>] AND/OR\n"
	"\t[--userid=<userid>] AND/OR\n"
	"\t[--maxmemalloc=<number of bytes>] AND/OR\n"
	"\t[--verbose]\n";

enum optdefs {
	/* commands */
	ENCRYPT = 260,
	DECRYPT,
	SIGN,
	CLEARSIGN,
	VERIFY,
	VERIFY_CAT,
	LIST_PACKETS,
	SHOW_KEYS,
	VERSION_CMD,
	HELP_CMD,

	/* options */
	SSHKEYS,
	KEYRING,
	USERID,
	ARMOUR,
	HOMEDIR,
	DETACHED,
	HASH_ALG,
	OUTPUT,
	RESULTS,
	VERBOSE,
	COREDUMPS,
	PASSWDFD,
	SSHKEYFILE,
	MAX_MEM_ALLOC,
	DURATION,
	BIRTHTIME,
	CIPHER,
	NUMTRIES,

	/* debug */
	OPS_DEBUG
};

#define EXIT_ERROR	2

static struct option options[] = {
	/* file manipulation commands */
	{"encrypt",	no_argument,		NULL,	ENCRYPT},
	{"decrypt",	no_argument,		NULL,	DECRYPT},
	{"sign",	no_argument,		NULL,	SIGN},
	{"clearsign",	no_argument,		NULL,	CLEARSIGN},
	{"verify",	no_argument,		NULL,	VERIFY},
	{"cat",		no_argument,		NULL,	VERIFY_CAT},
	{"vericat",	no_argument,		NULL,	VERIFY_CAT},
	{"verify-cat",	no_argument,		NULL,	VERIFY_CAT},
	{"verify-show",	no_argument,		NULL,	VERIFY_CAT},
	{"verifyshow",	no_argument,		NULL,	VERIFY_CAT},
	/* file listing commands */
	{"list-packets", no_argument,		NULL,	LIST_PACKETS},
	/* debugging commands */
	{"help",	no_argument,		NULL,	HELP_CMD},
	{"version",	no_argument,		NULL,	VERSION_CMD},
	{"debug",	required_argument, 	NULL,	OPS_DEBUG},
	{"show-keys",	no_argument, 		NULL,	SHOW_KEYS},
	{"showkeys",	no_argument, 		NULL,	SHOW_KEYS},
	/* options */
	{"ssh",		no_argument, 		NULL,	SSHKEYS},
	{"ssh-keys",	no_argument, 		NULL,	SSHKEYS},
	{"sshkeyfile",	required_argument, 	NULL,	SSHKEYFILE},
	{"coredumps",	no_argument, 		NULL,	COREDUMPS},
	{"keyring",	required_argument, 	NULL,	KEYRING},
	{"userid",	required_argument, 	NULL,	USERID},
	{"home",	required_argument, 	NULL,	HOMEDIR},
	{"homedir",	required_argument, 	NULL,	HOMEDIR},
	{"ascii",	no_argument,		NULL,	ARMOUR},
	{"armor",	no_argument,		NULL,	ARMOUR},
	{"armour",	no_argument,		NULL,	ARMOUR},
	{"detach",	no_argument,		NULL,	DETACHED},
	{"detached",	no_argument,		NULL,	DETACHED},
	{"hash-alg",	required_argument, 	NULL,	HASH_ALG},
	{"hash",	required_argument, 	NULL,	HASH_ALG},
	{"algorithm",	required_argument, 	NULL,	HASH_ALG},
	{"verbose",	no_argument, 		NULL,	VERBOSE},
	{"pass-fd",	required_argument, 	NULL,	PASSWDFD},
	{"output",	required_argument, 	NULL,	OUTPUT},
	{"results",	required_argument, 	NULL,	RESULTS},
	{"maxmemalloc",	required_argument, 	NULL,	MAX_MEM_ALLOC},
	{"max-mem",	required_argument, 	NULL,	MAX_MEM_ALLOC},
	{"max-alloc",	required_argument, 	NULL,	MAX_MEM_ALLOC},
	{"from",	required_argument, 	NULL,	BIRTHTIME},
	{"birth",	required_argument, 	NULL,	BIRTHTIME},
	{"birthtime",	required_argument, 	NULL,	BIRTHTIME},
	{"creation",	required_argument, 	NULL,	BIRTHTIME},
	{"duration",	required_argument, 	NULL,	DURATION},
	{"expiry",	required_argument, 	NULL,	DURATION},
	{"cipher",	required_argument, 	NULL,	CIPHER},
	{"num-tries",	required_argument, 	NULL,	NUMTRIES},
	{"numtries",	required_argument, 	NULL,	NUMTRIES},
	{"attempts",	required_argument, 	NULL,	NUMTRIES},
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
	*temp = NULL;
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
	const int	 cleartext = 1;
	unsigned	 maxsize;
	char		*out;
	char		*in;
	int		 ret;
	int		 cc;

	switch (p->cmd) {
	case ENCRYPT:
		if (f == NULL) {
			cc = stdin_to_mem(netpgp, &in, &out, &maxsize);
			ret = netpgp_encrypt_memory(netpgp,
					netpgp_getvar(netpgp, "userid"),
					in, cc, out, maxsize, p->armour);
			ret = show_output(out, ret, "Bad memory encryption");
			free(in);
			free(out);
			return ret;
		}
		return netpgp_encrypt_file(netpgp,
					netpgp_getvar(netpgp, "userid"),
					f, p->output,
					p->armour);
	case DECRYPT:
		if (f == NULL) {
			cc = stdin_to_mem(netpgp, &in, &out, &maxsize);
			ret = netpgp_decrypt_memory(netpgp, in, cc, out,
					maxsize, 0);
			ret = show_output(out, ret, "Bad memory decryption");
			free(in);
			free(out);
			return ret;
		}
		return netpgp_decrypt_file(netpgp, f, p->output, p->armour);
	case CLEARSIGN:
	case SIGN:
		if (f == NULL) {
			cc = stdin_to_mem(netpgp, &in, &out, &maxsize);
			ret = netpgp_sign_memory(netpgp,
					netpgp_getvar(netpgp, "userid"),
					in, cc, out,
					maxsize, p->armour,
					(p->cmd == CLEARSIGN) ? cleartext :
								!cleartext);
			ret = show_output(out, ret, "Bad memory signature");
			free(in);
			free(out);
			return ret;
		}
		return netpgp_sign_file(netpgp,
					netpgp_getvar(netpgp, "userid"),
					f, p->output,
					p->armour,
					(p->cmd == CLEARSIGN) ? cleartext :
								!cleartext,
					p->detached);
	case VERIFY:
	case VERIFY_CAT:
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
	case LIST_PACKETS:
		if (f == NULL) {
			(void) fprintf(stderr, "%s: No filename provided\n",
				p->progname);
			return 0;
		}
		return netpgp_list_packets(netpgp, f, p->armour, NULL);
	case SHOW_KEYS:
		return netpgp_validate_sigs(netpgp);
	case HELP_CMD:
	default:
		print_usage(usage, p->progname);
		exit(EXIT_SUCCESS);
	}
}

/* set an option */
static int
setoption(netpgp_t *netpgp, prog_t *p, int val, char *arg, int *homeset)
{
	switch (val) {
	case COREDUMPS:
		netpgp_setvar(netpgp, "coredumps", "allowed");
		break;
	case ENCRYPT:
		/* for encryption, we need a userid */
		netpgp_setvar(netpgp, "need userid", "1");
		p->cmd = val;
		break;
	case SIGN:
	case CLEARSIGN:
		/* for signing, we need a userid and a seckey */
		netpgp_setvar(netpgp, "need seckey", "1");
		netpgp_setvar(netpgp, "need userid", "1");
		p->cmd = val;
		break;
	case DECRYPT:
		/* for decryption, we need a seckey */
		netpgp_setvar(netpgp, "need seckey", "1");
		p->cmd = val;
		break;
	case VERIFY:
	case VERIFY_CAT:
	case LIST_PACKETS:
	case SHOW_KEYS:
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
		if (arg == NULL) {
			(void) fprintf(stderr,
				"No userid argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_setvar(netpgp, "userid", arg);
		break;
	case ARMOUR:
		p->armour = 1;
		break;
	case DETACHED:
		p->detached = 1;
		break;
	case VERBOSE:
		netpgp_incvar(netpgp, "verbose", 1);
		break;
	case HOMEDIR:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"No home directory argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_set_homedir(netpgp, arg, NULL, 0);
		*homeset = 1;
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
			"No pass-fd argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_setvar(netpgp, "pass-fd", arg);
		break;
	case OUTPUT:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"No output filename argument provided\n");
			exit(EXIT_ERROR);
		}
		if (p->output) {
			(void) free(p->output);
		}
		p->output = strdup(arg);
		break;
	case RESULTS:
		if (arg == NULL) {
			(void) fprintf(stderr,
			"No output filename argument provided\n");
			exit(EXIT_ERROR);
		}
		netpgp_setvar(netpgp, "results", arg);
		break;
	case SSHKEYFILE:
		netpgp_setvar(netpgp, "ssh keys", "1");
		netpgp_setvar(netpgp, "sshkeyfile", arg);
		break;
	case MAX_MEM_ALLOC:
		netpgp_setvar(netpgp, "max mem alloc", arg);
		break;
	case DURATION:
		netpgp_setvar(netpgp, "duration", arg);
		break;
	case BIRTHTIME:
		netpgp_setvar(netpgp, "birthtime", arg);
		break;
	case CIPHER:
		netpgp_setvar(netpgp, "cipher", arg);
		break;
	case NUMTRIES:
		netpgp_setvar(netpgp, "numtries", arg);
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
	netpgp_t	netpgp;
	prog_t          p;
	int             homeset;
	int             optindex;
	int             ret;
	int             ch;
	int             i;

	(void) memset(&p, 0x0, sizeof(p));
	(void) memset(&netpgp, 0x0, sizeof(netpgp));
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
	homeset = 0;
	optindex = 0;
	while ((ch = getopt_long(argc, argv, "S:Vdeo:sv", options, &optindex)) != -1) {
		if (ch >= ENCRYPT) {
			/* getopt_long returns 0 for long options */
			if (!setoption(&netpgp, &p, options[optindex].val, optarg, &homeset)) {
				(void) fprintf(stderr, "Bad option\n");
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
			case 'd':
				/* for decryption, we need the seckey */
				netpgp_setvar(&netpgp, "need seckey", "1");
				p.cmd = DECRYPT;
				break;
			case 'e':
				/* for encryption, we need a userid */
				netpgp_setvar(&netpgp, "need userid", "1");
				p.cmd = ENCRYPT;
				break;
			case 'o':
				if (!parse_option(&netpgp, &p, optarg, &homeset)) {
					(void) fprintf(stderr, "Bad option\n");
				}
				break;
			case 's':
				/* for signing, we need a userid and a seckey */
				netpgp_setvar(&netpgp, "need seckey", "1");
				netpgp_setvar(&netpgp, "need userid", "1");
				p.cmd = SIGN;
				break;
			case 'v':
				p.cmd = VERIFY;
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
