/*
 * Copyright (C) 2004, 2005, 2007-2009, 2011  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 2001, 2003  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: rndc-confgen.c,v 1.5.308.2 2011-03-12 04:59:13 tbox Exp $ */

/*! \file */

/**
 * rndc-confgen generates configuration files for rndc. It can be used
 * as a convenient alternative to writing the rndc.conf file and the
 * corresponding controls and key statements in named.conf by hand.
 * Alternatively, it can be run with the -a option to set up a
 * rndc.key file and avoid the need for a rndc.conf file and a
 * controls statement altogether.
 */

#include <config.h>

#include <stdlib.h>
#include <stdarg.h>

#include <isc/assertions.h>
#include <isc/base64.h>
#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/entropy.h>
#include <isc/file.h>
#include <isc/keyboard.h>
#include <isc/mem.h>
#include <isc/net.h>
#include <isc/print.h>
#include <isc/result.h>
#include <isc/string.h>
#include <isc/time.h>
#include <isc/util.h>

#include <dns/keyvalues.h>
#include <dns/name.h>

#include <dst/dst.h>
#include <confgen/os.h>

#include "util.h"
#include "keygen.h"

#define DEFAULT_KEYLENGTH	128		/*% Bits. */
#define DEFAULT_KEYNAME		"rndc-key"
#define DEFAULT_SERVER		"127.0.0.1"
#define DEFAULT_PORT		953

static char program[256];
const char *progname;

isc_boolean_t verbose = ISC_FALSE;

const char *keyfile, *keydef;

ISC_PLATFORM_NORETURN_PRE static void
usage(int status) ISC_PLATFORM_NORETURN_POST;

static void
usage(int status) {

	fprintf(stderr, "\
Usage:\n\
 %s [-a] [-b bits] [-c keyfile] [-k keyname] [-p port] [-r randomfile] \
[-s addr] [-t chrootdir] [-u user]\n\
  -a:		 generate just the key clause and write it to keyfile (%s)\n\
  -b bits:	 from 1 through 512, default %d; total length of the secret\n\
  -c keyfile:	 specify an alternate key file (requires -a)\n\
  -k keyname:	 the name as it will be used  in named.conf and rndc.conf\n\
  -p port:	 the port named will listen on and rndc will connect to\n\
  -r randomfile: source of random data (use \"keyboard\" for key timing)\n\
  -s addr:	 the address to which rndc should connect\n\
  -t chrootdir:	 write a keyfile in chrootdir as well (requires -a)\n\
  -u user:	 set the keyfile owner to \"user\" (requires -a)\n",
		 progname, keydef, DEFAULT_KEYLENGTH);

	exit (status);
}

int
main(int argc, char **argv) {
	isc_boolean_t show_final_mem = ISC_FALSE;
	isc_buffer_t key_txtbuffer;
	char key_txtsecret[256];
	isc_mem_t *mctx = NULL;
	isc_result_t result = ISC_R_SUCCESS;
	const char *keyname = NULL;
	const char *randomfile = NULL;
	const char *serveraddr = NULL;
	dns_secalg_t alg = DST_ALG_HMACMD5;
	const char *algname = alg_totext(alg);
	char *p;
	int ch;
	int port;
	int keysize;
	struct in_addr addr4_dummy;
	struct in6_addr addr6_dummy;
	char *chrootdir = NULL;
	char *user = NULL;
	isc_boolean_t keyonly = ISC_FALSE;
	int len;

	keydef = keyfile = RNDC_KEYFILE;

	result = isc_file_progname(*argv, program, sizeof(program));
	if (result != ISC_R_SUCCESS)
		memcpy(program, "rndc-confgen", 13);
	progname = program;

	keyname = DEFAULT_KEYNAME;
	keysize = DEFAULT_KEYLENGTH;
	serveraddr = DEFAULT_SERVER;
	port = DEFAULT_PORT;

	isc_commandline_errprint = ISC_FALSE;

	while ((ch = isc_commandline_parse(argc, argv,
					   "ab:c:hk:Mmp:r:s:t:u:Vy")) != -1) {
		switch (ch) {
		case 'a':
			keyonly = ISC_TRUE;
			break;
		case 'b':
			keysize = strtol(isc_commandline_argument, &p, 10);
			if (*p != '\0' || keysize < 0)
				fatal("-b requires a non-negative number");
			if (keysize < 1 || keysize > 512)
				fatal("-b must be in the range 1 through 512");
			break;
		case 'c':
			keyfile = isc_commandline_argument;
			break;
		case 'h':
			usage(0);
		case 'k':
		case 'y':	/* Compatible with rndc -y. */
			keyname = isc_commandline_argument;
			break;
		case 'M':
			isc_mem_debugging = ISC_MEM_DEBUGTRACE;
			break;

		case 'm':
			show_final_mem = ISC_TRUE;
			break;
		case 'p':
			port = strtol(isc_commandline_argument, &p, 10);
			if (*p != '\0' || port < 0 || port > 65535)
				fatal("port '%s' out of range",
				      isc_commandline_argument);
			break;
		case 'r':
			randomfile = isc_commandline_argument;
			break;
		case 's':
			serveraddr = isc_commandline_argument;
			if (inet_pton(AF_INET, serveraddr, &addr4_dummy) != 1 &&
			    inet_pton(AF_INET6, serveraddr, &addr6_dummy) != 1)
				fatal("-s should be an IPv4 or IPv6 address");
			break;
		case 't':
			chrootdir = isc_commandline_argument;
			break;
		case 'u':
			user = isc_commandline_argument;
			break;
		case 'V':
			verbose = ISC_TRUE;
			break;
		case '?':
			if (isc_commandline_option != '?') {
				fprintf(stderr, "%s: invalid argument -%c\n",
					program, isc_commandline_option);
				usage(1);
			} else
				usage(0);
			break;
		default:
			fprintf(stderr, "%s: unhandled option -%c\n",
				program, isc_commandline_option);
			exit(1);
		}
	}

	argc -= isc_commandline_index;
	argv += isc_commandline_index;
	POST(argv);

	if (argc > 0)
		usage(1);

	DO("create memory context", isc_mem_create(0, 0, &mctx));
	isc_buffer_init(&key_txtbuffer, &key_txtsecret, sizeof(key_txtsecret));

	generate_key(mctx, randomfile, alg, keysize, &key_txtbuffer);

	if (keyonly) {
		write_key_file(keyfile, chrootdir == NULL ? user : NULL,
			       keyname, &key_txtbuffer, alg);

		if (chrootdir != NULL) {
			char *buf;
			len = strlen(chrootdir) + strlen(keyfile) + 2;
			buf = isc_mem_get(mctx, len);
			if (buf == NULL)
				fatal("isc_mem_get(%d) failed\n", len);
			snprintf(buf, len, "%s%s%s", chrootdir,
				 (*keyfile != '/') ? "/" : "", keyfile);

			write_key_file(buf, user, keyname, &key_txtbuffer, alg);
			isc_mem_put(mctx, buf, len);
		}
	} else {
		printf("\
# Start of rndc.conf\n\
key \"%s\" {\n\
	algorithm %s;\n\
	secret \"%.*s\";\n\
};\n\
\n\
options {\n\
	default-key \"%s\";\n\
	default-server %s;\n\
	default-port %d;\n\
};\n\
# End of rndc.conf\n\
\n\
# Use with the following in named.conf, adjusting the allow list as needed:\n\
# key \"%s\" {\n\
# 	algorithm %s;\n\
# 	secret \"%.*s\";\n\
# };\n\
# \n\
# controls {\n\
# 	inet %s port %d\n\
# 		allow { %s; } keys { \"%s\"; };\n\
# };\n\
# End of named.conf\n",
		       keyname, algname,
		       (int)isc_buffer_usedlength(&key_txtbuffer),
		       (char *)isc_buffer_base(&key_txtbuffer),
		       keyname, serveraddr, port,
		       keyname, algname,
		       (int)isc_buffer_usedlength(&key_txtbuffer),
		       (char *)isc_buffer_base(&key_txtbuffer),
		       serveraddr, port, serveraddr, keyname);
	}

	if (show_final_mem)
		isc_mem_stats(mctx, stderr);

	isc_mem_destroy(&mctx);

	return (0);
}
