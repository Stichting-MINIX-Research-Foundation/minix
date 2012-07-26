#ifndef lint
static char *rcsid = "$Id: idnconv.c,v 1.1.1.1 2003-06-04 00:27:07 marka Exp $";
#endif

/*
 * Copyright (c) 2000,2001,2002 Japan Network Information Center.
 * All rights reserved.
 *  
 * By using this file, you agree to the terms and conditions set forth bellow.
 * 
 * 			LICENSE TERMS AND CONDITIONS 
 * 
 * The following License Terms and Conditions apply, unless a different
 * license is obtained from Japan Network Information Center ("JPNIC"),
 * a Japanese association, Kokusai-Kougyou-Kanda Bldg 6F, 2-3-4 Uchi-Kanda,
 * Chiyoda-ku, Tokyo 101-0047, Japan.
 * 
 * 1. Use, Modification and Redistribution (including distribution of any
 *    modified or derived work) in source and/or binary forms is permitted
 *    under this License Terms and Conditions.
 * 
 * 2. Redistribution of source code must retain the copyright notices as they
 *    appear in each source code file, this License Terms and Conditions.
 * 
 * 3. Redistribution in binary form must reproduce the Copyright Notice,
 *    this License Terms and Conditions, in the documentation and/or other
 *    materials provided with the distribution.  For the purposes of binary
 *    distribution the "Copyright Notice" refers to the following language:
 *    "Copyright (c) 2000-2002 Japan Network Information Center.  All rights reserved."
 * 
 * 4. The name of JPNIC may not be used to endorse or promote products
 *    derived from this Software without specific prior written approval of
 *    JPNIC.
 * 
 * 5. Disclaimer/Limitation of Liability: THIS SOFTWARE IS PROVIDED BY JPNIC
 *    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 *    PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL JPNIC BE LIABLE
 *    FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *    SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *    BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *    WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 *    OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 *    ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 */

/*
 * idnconv -- Codeset converter for named.conf and zone files
 */

#include <config.h>

#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#ifdef HAVE_LOCALE_H
#include <locale.h>
#endif

#include <idn/result.h>
#include <idn/converter.h>
#include <idn/normalizer.h>
#include <idn/utf8.h>
#include <idn/resconf.h>
#include <idn/res.h>
#include <idn/util.h>
#include <idn/version.h>

#include "util.h"

#define MAX_DELIMITER		10
#define MAX_LOCALMAPPER		10
#define MAX_MAPPER		10
#define MAX_NORMALIZER		10
#define MAX_CHEKER		10

#define FLAG_REVERSE		0x0001
#define FLAG_DELIMMAP		0x0002
#define FLAG_LOCALMAP		0x0004
#define FLAG_MAP		0x0008
#define FLAG_NORMALIZE		0x0010
#define FLAG_PROHIBITCHECK	0x0020
#define FLAG_UNASSIGNCHECK	0x0040
#define FLAG_BIDICHECK		0x0080
#define FLAG_ASCIICHECK		0x0100
#define FLAG_LENGTHCHECK	0x0200
#define FLAG_ROUNDTRIPCHECK	0x0400
#define FLAG_SELECTIVE		0x0800

#define FLAG_NAMEPREP \
	(FLAG_MAP|FLAG_NORMALIZE|FLAG_PROHIBITCHECK|FLAG_UNASSIGNCHECK|\
	 FLAG_BIDICHECK)

#define DEFAULT_FLAGS \
	(FLAG_LOCALMAP|FLAG_NAMEPREP|FLAG_ASCIICHECK|FLAG_LENGTHCHECK|\
	FLAG_ROUNDTRIPCHECK|FLAG_SELECTIVE|FLAG_DELIMMAP)

int		line_number;		/* current input file line number */
static int	flush_every_line = 0;	/* pretty obvious */

static int		encode_file(idn_resconf_t conf1, idn_resconf_t conf2, 
				    FILE *fp, int flags);
static int		decode_file(idn_resconf_t conf1, idn_resconf_t conf2, 
				    FILE *fp, int flags);
static int		trim_newline(idnconv_strbuf_t *buf);
static idn_result_t	convert_line(idnconv_strbuf_t *from,
				     idnconv_strbuf_t *to,
				     idn_resconf_t conf,
				     idn_action_t actions, int flags);
static void		print_usage(char *cmd);
static void		print_version(void);
static unsigned long	get_ucs(const char *p);

int
main(int ac, char **av) {
	char *cmd = *av;
	char *cname;
	unsigned long delimiters[MAX_DELIMITER];
	char *localmappers[MAX_LOCALMAPPER];
	char *nameprep_version = NULL;
	int ndelimiters = 0;
	int nlocalmappers = 0;
	char *in_code = NULL;
	char *out_code = NULL;
	char *resconf_file = NULL;
	int no_resconf = 0;
	char *encoding_alias = NULL;
	int flags = DEFAULT_FLAGS;
	FILE *fp;
	idn_result_t r;
	idn_resconf_t resconf1, resconf2;
	idn_converter_t conv;
	int exit_value;

#ifdef HAVE_SETLOCALE
	(void)setlocale(LC_ALL, "");
#endif

	/*
	 * If the command name begins with 'r', reverse mode is assumed.
	 */
	if ((cname = strrchr(cmd, '/')) != NULL)
		cname++;
	else
		cname = cmd;
	if (cname[0] == 'r')
		flags |= FLAG_REVERSE;

	ac--;
	av++;
	while (ac > 0 && **av == '-') {

#define OPT_MATCH(opt) (strcmp(*av, opt) == 0)
#define MUST_HAVE_ARG if (ac < 2) print_usage(cmd)
#define APPEND_LIST(array, size, item, what) \
	if (size >= (sizeof(array) / sizeof(array[0]))) { \
		errormsg("too many " what "\n"); \
		exit(1); \
	} \
	array[size++] = item; \
	ac--; av++

		if (OPT_MATCH("-in") || OPT_MATCH("-i")) {
			MUST_HAVE_ARG;
			in_code = av[1];
			ac--;
			av++;
		} else if (OPT_MATCH("-out") || OPT_MATCH("-o")) {
			MUST_HAVE_ARG;
			out_code = av[1];
			ac--;
			av++;
		} else if (OPT_MATCH("-conf") || OPT_MATCH("-c")) {
			MUST_HAVE_ARG;
			resconf_file = av[1];
			ac--;
			av++;
		} else if (OPT_MATCH("-nameprep") || OPT_MATCH("-n")) {
			MUST_HAVE_ARG;
			nameprep_version = av[1];
			ac--;
			av++;
		} else if (OPT_MATCH("-noconf") || OPT_MATCH("-C")) {
			no_resconf = 1;
		} else if (OPT_MATCH("-reverse") || OPT_MATCH("-r")) {
			flags |= FLAG_REVERSE;
		} else if (OPT_MATCH("-nolocalmap") || OPT_MATCH("-L")) {
			flags &= ~FLAG_LOCALMAP;
		} else if (OPT_MATCH("-nonameprep") || OPT_MATCH("-N")) {
			flags &= ~FLAG_NAMEPREP;
		} else if (OPT_MATCH("-unassigncheck") || OPT_MATCH("-u")) {
			flags |= FLAG_UNASSIGNCHECK;
		} else if (OPT_MATCH("-nounassigncheck") || OPT_MATCH("-U")) {
			flags &= ~FLAG_UNASSIGNCHECK;
		} else if (OPT_MATCH("-nobidicheck") || OPT_MATCH("-B")) {
			flags &= ~FLAG_BIDICHECK;
		} else if (OPT_MATCH("-noasciicheck") || OPT_MATCH("-A")) {
			flags &= ~FLAG_ASCIICHECK;
		} else if (OPT_MATCH("-nolengthcheck")) {
			flags &= ~FLAG_LENGTHCHECK;
		} else if (OPT_MATCH("-noroundtripcheck")) {
			flags &= ~FLAG_ROUNDTRIPCHECK;
		} else if (OPT_MATCH("-whole") || OPT_MATCH("-w")) {
			flags &= ~FLAG_SELECTIVE;
		} else if (OPT_MATCH("-localmap")) {
			MUST_HAVE_ARG;
			APPEND_LIST(localmappers, nlocalmappers, av[1],
				    "local maps");
		} else if (OPT_MATCH("-delimiter")) {
			unsigned long v;
			MUST_HAVE_ARG;
			v = get_ucs(av[1]);
			APPEND_LIST(delimiters, ndelimiters, v,
				    "delimiter maps");
		} else if (OPT_MATCH("-alias") || OPT_MATCH("-a")) {
			MUST_HAVE_ARG;
			encoding_alias = av[1];
			ac--;
			av++;
		} else if (OPT_MATCH("-flush")) {
			flush_every_line = 1;
		} else if (OPT_MATCH("-version") || OPT_MATCH("-v")) {
			print_version();
		} else {
			print_usage(cmd);
		}
#undef OPT_MATCH
#undef MUST_HAVE_ARG
#undef APPEND_LIST

		ac--;
		av++;
	}

	if (ac > 1)
		print_usage(cmd);

	/* Initialize. */
	if ((r = idn_resconf_initialize()) != idn_success) {
		errormsg("error initializing library\n");
		return (1);
	}

	/*
	 * Create resource contexts.
	 * `resconf1' and `resconf2' are almost the same but local and
	 * IDN encodings are reversed.
	 */
	resconf1 = NULL;
	resconf2 = NULL;
	if (idn_resconf_create(&resconf1) != idn_success ||
	    idn_resconf_create(&resconf2) != idn_success) {
		errormsg("error initializing configuration contexts\n");
		return (1);
	}

	/* Load configuration file. */
	if (no_resconf) {
		set_defaults(resconf1);
		set_defaults(resconf2);
	} else {
		load_conf_file(resconf1, resconf_file);
		load_conf_file(resconf2, resconf_file);
	}

	/* Set encoding alias file. */
	if (encoding_alias != NULL)
		set_encoding_alias(encoding_alias);

	/* Set input codeset. */
	if (flags & FLAG_REVERSE) {
		if (in_code == NULL) {
			conv = idn_resconf_getidnconverter(resconf1);
			if (conv == NULL) {
				errormsg("cannot get the IDN encoding.\n"
					 "please specify an appropriate one "
			 		 "with `-in' option.\n");
				exit(1);
			}
			idn_resconf_setlocalconverter(resconf2, conv);
			idn_converter_destroy(conv);
		} else {
			set_idncode(resconf1, in_code);
			set_localcode(resconf2, in_code);
		}
	} else {
		if (in_code == NULL) {
			conv = idn_resconf_getlocalconverter(resconf1);
			if (conv == NULL) {
				errormsg("cannot get the local encoding.\n"
					 "please specify an appropriate one "
			 		 "with `-in' option.\n");
				exit(1);
			}
			idn_resconf_setidnconverter(resconf2, conv);
			idn_converter_destroy(conv);
		} else {
			set_localcode(resconf1, in_code);
			set_idncode(resconf2, in_code);
		}
	}

	/* Set output codeset. */
	if (flags & FLAG_REVERSE) {
		if (out_code == NULL) {
			conv = idn_resconf_getlocalconverter(resconf1);
			if (conv == NULL) {
				errormsg("cannot get the local encoding.\n"
					 "please specify an appropriate one "
			 		 "with `-out' option.\n");
				exit(1);
			}
			idn_resconf_setidnconverter(resconf2, conv);
			idn_converter_destroy(conv);
		} else {
			set_localcode(resconf1, out_code);
			set_idncode(resconf2, out_code);
		}
	} else {
		if (out_code == NULL) {
			conv = idn_resconf_getidnconverter(resconf1);
			if (conv == NULL) {
				errormsg("cannot get the IDN encoding.\n"
					 "please specify an appropriate one "
			 		 "with `-out' option.\n");
				exit(1);
			}
			idn_resconf_setlocalconverter(resconf2, conv);
			idn_converter_destroy(conv);
		} else {
			set_idncode(resconf1, out_code);
			set_localcode(resconf2, out_code);
		}
	}

	/* Set delimiter map(s). */
	if (ndelimiters > 0) {
		set_delimitermapper(resconf1, delimiters, ndelimiters);
		set_delimitermapper(resconf2, delimiters, ndelimiters);
	}

	/* Set local map(s). */
	if (nlocalmappers > 0) {
		set_localmapper(resconf1, localmappers, nlocalmappers);
		set_localmapper(resconf2, localmappers, nlocalmappers);
	}

	/* Set NAMEPREP version. */
	if (nameprep_version != NULL) {
		set_nameprep(resconf1, nameprep_version);
		set_nameprep(resconf2, nameprep_version);
	}

	idn_res_enable(1);

	/* Open input file. */
	if (ac > 0) {
		if ((fp = fopen(av[0], "r")) == NULL) {
			errormsg("cannot open file %s: %s\n",
				 av[0], strerror(errno));
			return (1);
		}
	} else {
		fp = stdin;
	}

	/* Do the conversion. */
	if (flags & FLAG_REVERSE)
		exit_value = decode_file(resconf1, resconf2, fp, flags);
	else
		exit_value = encode_file(resconf1, resconf2, fp, flags);

	idn_resconf_destroy(resconf1);
	idn_resconf_destroy(resconf2);

	return exit_value;
}

static int
encode_file(idn_resconf_t conf1, idn_resconf_t conf2, FILE *fp, int flags) {
	idn_result_t r;
	idnconv_strbuf_t buf1, buf2;
	idn_action_t actions1, actions2;
	int nl_trimmed;
	int local_ace_hack;
	idn_converter_t conv;

	/*
	 * See if the input codeset is an ACE.
	 */
	conv = idn_resconf_getlocalconverter(conf1);
	if (conv != NULL && idn_converter_isasciicompatible(conv) &&
	    (flags & FLAG_SELECTIVE))
		local_ace_hack = 1;
	else
		local_ace_hack = 0;
	if (conv != NULL)
		idn_converter_destroy(conv);

	if (local_ace_hack) {
		actions1 = IDN_IDNCONV;
		if (flags & FLAG_ROUNDTRIPCHECK)
			actions1 |= IDN_RTCHECK;
	} else {
		actions1 = IDN_LOCALCONV;
	}

	actions2 = IDN_IDNCONV;
	if (flags & FLAG_DELIMMAP)
		actions2 |= IDN_DELIMMAP;
	if (flags & FLAG_LOCALMAP)
		actions2 |= IDN_LOCALMAP;
	if (flags & FLAG_MAP)
		actions2 |= IDN_MAP;
	if (flags & FLAG_NORMALIZE)
		actions2 |= IDN_NORMALIZE;
	if (flags & FLAG_PROHIBITCHECK)
		actions2 |= IDN_PROHCHECK;
	if (flags & FLAG_UNASSIGNCHECK)
		actions2 |= IDN_UNASCHECK;
	if (flags & FLAG_BIDICHECK)
		actions2 |= IDN_BIDICHECK;
	if (flags & FLAG_ASCIICHECK)
		actions2 |= IDN_ASCCHECK;
	if (flags & FLAG_LENGTHCHECK)
		actions2 |= IDN_LENCHECK;

	strbuf_init(&buf1);
	strbuf_init(&buf2);
	line_number = 1;
	while (strbuf_getline(&buf1, fp) != NULL) {
		/*
		 * Trim newline at the end.  This is needed for
		 * those ascii-comatible encodings such as UTF-5 or RACE
		 * not to try converting newlines, which will result
		 * in `invalid encoding' error.
		 */
		nl_trimmed = trim_newline(&buf1);

		/*
		 * Convert input line to UTF-8.
		 */
		if (local_ace_hack)
			r = convert_line(&buf1, &buf2, conf2, actions1,
					 FLAG_REVERSE|FLAG_SELECTIVE);
		else
			r = convert_line(&buf1, &buf2, conf1, actions1,
					 0);
				 
		if (r != idn_success) {
			errormsg("conversion failed at line %d: %s\n",
				 line_number,
				 idn_result_tostring(r));
			goto error;
		}
		if (!idn_utf8_isvalidstring(strbuf_get(&buf2))) {
			errormsg("conversion to utf-8 failed at line %d\n",
				 line_number);
			goto error;
		}

		/*
		 * Perform local mapping and NAMEPREP, and convert to
		 * the output codeset.
		 */
		r = convert_line(&buf2, &buf1, conf1, actions2,
				 flags & FLAG_SELECTIVE);
				 
		if (r != idn_success) {
			errormsg("error in nameprep or output conversion "
				 "at line %d: %s\n",
				 line_number, idn_result_tostring(r));
			goto error;
		}

		fputs(strbuf_get(&buf1), stdout);
		if (nl_trimmed)
			putc('\n', stdout);

		if (flush_every_line)
			fflush(stdout);

		line_number++;
	}

	strbuf_reset(&buf1);
	strbuf_reset(&buf2);
	return (0);

 error:
	strbuf_reset(&buf1);
	strbuf_reset(&buf2);
	return (1);
}

static int
decode_file(idn_resconf_t conf1, idn_resconf_t conf2, FILE *fp, int flags) {
	idn_result_t r;
	idnconv_strbuf_t buf1, buf2;
	idn_action_t actions1, actions2;
	int nl_trimmed;
	int local_ace_hack, idn_ace_hack;
	idn_converter_t conv;

	/*
	 * See if the input codeset is an ACE.
	 */
	conv = idn_resconf_getidnconverter(conf1);
	if (conv != NULL && idn_converter_isasciicompatible(conv) &&
	    (flags & FLAG_SELECTIVE))
		idn_ace_hack = 1;
	else
		idn_ace_hack = 0;
	if (conv != NULL)
		idn_converter_destroy(conv);

	conv = idn_resconf_getlocalconverter(conf1);
	if (conv != NULL && idn_converter_isasciicompatible(conv) &&
	    (flags & FLAG_SELECTIVE))
		local_ace_hack = 1;
	else
		local_ace_hack = 0;
	if (conv != NULL)
		idn_converter_destroy(conv);

	actions1 = IDN_IDNCONV;

	if (local_ace_hack) {
		actions2 = IDN_IDNCONV;
		if (flags & FLAG_MAP)
			actions2 |= IDN_MAP;
		if (flags & FLAG_NORMALIZE)
			actions2 |= IDN_NORMALIZE;
		if (flags & FLAG_PROHIBITCHECK)
			actions2 |= IDN_PROHCHECK;
		if (flags & FLAG_UNASSIGNCHECK)
			actions2 |= IDN_UNASCHECK;
		if (flags & FLAG_BIDICHECK)
			actions2 |= IDN_BIDICHECK;
		if (flags & FLAG_ASCIICHECK)
			actions2 |= IDN_ASCCHECK;
		if (flags & FLAG_LENGTHCHECK)
			actions2 |= IDN_LENCHECK;
	} else {
		actions2 = IDN_LOCALCONV;
	}

	if (flags & FLAG_DELIMMAP)
		actions1 |= IDN_DELIMMAP;
	if (flags & FLAG_MAP)
		actions1 |= IDN_MAP;
	if (flags & FLAG_NORMALIZE)
		actions1 |= IDN_NORMALIZE;
	if (flags & FLAG_NORMALIZE)
		actions1 |= IDN_NORMALIZE;
	if (flags & FLAG_PROHIBITCHECK)
		actions1 |= IDN_PROHCHECK;
	if (flags & FLAG_UNASSIGNCHECK)
		actions1 |= IDN_UNASCHECK;
	if (flags & FLAG_BIDICHECK)
		actions1 |= IDN_BIDICHECK;
	if (flags & FLAG_ASCIICHECK)
		actions1 |= IDN_ASCCHECK;
	if (flags & FLAG_ROUNDTRIPCHECK)
		actions1 |= IDN_RTCHECK;

	strbuf_init(&buf1);
	strbuf_init(&buf2);
	line_number = 1;
	while (strbuf_getline(&buf1, fp) != NULL) {
		/*
		 * Trim newline at the end.  This is needed for
		 * those ascii-comatible encodings such as UTF-5 or RACE
		 * not to try converting newlines, which will result
		 * in `invalid encoding' error.
		 */
		nl_trimmed = trim_newline(&buf1);

		/*
		 * Treat input line as the string encoded in local
		 * encoding and convert it to UTF-8 encoded string.
		 */
		if (local_ace_hack) {
			if (strbuf_copy(&buf2, strbuf_get(&buf1)) == NULL)
				r = idn_nomemory;
			else
				r = idn_success;
		} else {
			r = convert_line(&buf1, &buf2, conf1, IDN_LOCALCONV,
					 0);
		}
		if (r != idn_success) {
			errormsg("conversion failed at line %d: %s\n",
				 line_number, idn_result_tostring(r));
			goto error;
		}

		/*
		 * Convert internationalized domain names in the line.
		 */
		if (idn_ace_hack) {
			r = convert_line(&buf2, &buf1, conf1, actions1,
					 FLAG_REVERSE|FLAG_SELECTIVE);
		} else {
			r = convert_line(&buf2, &buf1, conf1, actions1,
					 FLAG_REVERSE);
		}
		if (r != idn_success) {
			errormsg("conversion failed at line %d: %s\n",
				 line_number,
				 idn_result_tostring(r));
			goto error;
		}
		if (!idn_utf8_isvalidstring(strbuf_get(&buf1))) {
			errormsg("conversion to utf-8 failed at line %d\n",
				 line_number);
			goto error;
		}

		/*
		 * Perform round trip check and convert to the output
		 * codeset.
		 */
		if (local_ace_hack) {
			r = convert_line(&buf1, &buf2, conf2, actions2,
					 FLAG_SELECTIVE);
		} else {
			r = convert_line(&buf1, &buf2, conf1, actions2,
					 FLAG_REVERSE);
		}

		if (r != idn_success) {
			errormsg("error in nameprep or output conversion "
				 "at line %d: %s\n",
				 line_number, idn_result_tostring(r));
			goto error;
		}

		fputs(strbuf_get(&buf2), stdout);
		if (nl_trimmed)
			putc('\n', stdout);

		if (flush_every_line)
			fflush(stdout);

		line_number++;
	}
	strbuf_reset(&buf1);
	strbuf_reset(&buf2);
	return (0);

 error:
	strbuf_reset(&buf1);
	strbuf_reset(&buf2);
	return (1);
}

static int
trim_newline(idnconv_strbuf_t *buf) {
	/*
	 * If the string in BUF ends with a newline, trim it and
	 * return 1.  Otherwise, just return 0 without modifying BUF.
	 */
	char *s = strbuf_get(buf);
	size_t len = strlen(s);

	if (s[len - 1] == '\n') {
		s[len - 1] = '\0';
		return (1);
	}

	return (0);
}

static idn_result_t
convert_line(idnconv_strbuf_t *from, idnconv_strbuf_t *to,
	     idn_resconf_t conf, idn_action_t actions, int flags)
{
	idn_result_t r = idn_success;
	char *from_str = strbuf_get(from);

	for (;;) {
		char *to_str = strbuf_get(to);
		size_t to_size = strbuf_size(to);

		switch (flags & (FLAG_REVERSE|FLAG_SELECTIVE)) {
		case 0:
			r = idn_res_encodename(conf, actions, from_str,
					       to_str, to_size);
			break;
		case FLAG_REVERSE:
			r = idn_res_decodename(conf, actions, from_str,
					       to_str, to_size);
			break;
		case FLAG_SELECTIVE:
			r = selective_encode(conf, actions, from_str,
					     to_str, to_size);
			break;
		case FLAG_REVERSE|FLAG_SELECTIVE:
			r = selective_decode(conf, actions, from_str,
					     to_str, to_size);
			break;
		}
		if (r == idn_buffer_overflow) {
			/*
			 * Conversion is not successful because
			 * the size of the target buffer is not enough.
			 * Double the size and retry.
			 */
			if (strbuf_double(to) == NULL) {
				/* oops. allocation failed. */
				return (idn_nomemory);
			}
		} else {
			break;
		}
	}
	return (r);
}

static char *options[] = {
	"-in INPUT-CODESET   : specifies input codeset name.",
	"-i INPUT-CODESET    : synonym for -in",
	"-out OUTPUT-CODESET : specifies output codeset name.",
	"-o OUTPUT-CODESET   : synonym for -out",
	"-conf CONF-FILE     : specifies idnkit configuration file.",
	"-c CONF-FILE        : synonym for -conf",
	"-noconf             : do not load idnkit configuration file.",
	"-C                  : synonym for -noconf",
	"-reverse            : specifies reverse conversion.",
	"                      (i.e. IDN encoding to local encoding)",
	"-r                  : synonym for -reverse",
	"-nameprep VERSION   : specifies version name of NAMEPREP.",
	"-n VERSION          : synonym for -nameprep",
	"-nonameprep         : do not perform NAMEPREP.",
	"-N                  : synonym for -nonameprep",
	"-localmap MAPPING   : specifies local mapping.",
	"-nolocalmap         : do not perform local mapping.",
	"-L                  : synonym for -nolocalmap",
	"-nounassigncheck    : do not perform unassigned codepoint check.",
	"-U                  : synonym for -nounassigncheck",
	"-nobidicheck        : do not perform bidirectional text check.",
	"-B                  : synonym for -nobidicheck",
	"-nolengthcheck      : do not check label length.",
	"-noasciicheck       : do not check ASCII range characters.",
	"-A                  : synonym for -noasciicheck",
	"-noroundtripcheck   : do not perform round trip check.",
	"-delimiter U+XXXX   : specifies local delimiter code point.",
	"-alias alias-file   : specifies codeset alias file.",
	"-a                  : synonym for -alias",
	"-flush              : line-buffering mode.",
	"-whole              : convert the whole region instead of",
	"                      regions containing non-ascii characters.",
	"-w                  : synonym for -whole",
	"-version            : print version number, then exit.",
	"-v                  : synonym for -version",
	"",
	" The following options can be specified multiple times",
	"   -localmap, -delimiter",
	NULL,
};

static void
print_version() {
	fprintf(stderr, "idnconv (idnkit) version: %s\n"
		"library version: %s\n",
		IDNKIT_VERSION,
		idn_version_getstring());
	exit(0);
}

static void
print_usage(char *cmd) {
	int i;

	fprintf(stderr, "Usage: %s [options..] [file]\n", cmd);

	for (i = 0; options[i] != NULL; i++)
		fprintf(stderr, "\t%s\n", options[i]);

	exit(1);
}

static unsigned long
get_ucs(const char *p) {
	unsigned long v;
	char *end;

	/* Skip optional 'U+' */
	if (strncmp(p, "U+", 2) == 0)
		p += 2;

	v = strtoul(p, &end, 16);
	if (*end != '\0') {
		fprintf(stderr, "invalid UCS code point \"%s\"\n", p);
		exit(1);
	}

	return v;
}
