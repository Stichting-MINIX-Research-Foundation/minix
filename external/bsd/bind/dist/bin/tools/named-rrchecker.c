/*	$NetBSD: named-rrchecker.c,v 1.1.1.3 2014/12/10 03:34:31 christos Exp $	*/

/*
 * Copyright (C) 2013  Internet Systems Consortium, Inc. ("ISC")
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

#include <config.h>

#include <stdlib.h>

#include <isc/buffer.h>
#include <isc/commandline.h>
#include <isc/lex.h>
#include <isc/mem.h>
#include <isc/string.h>
#include <isc/util.h>

#include <dns/fixedname.h>
#include <dns/name.h>
#include <dns/rdata.h>
#include <dns/rdataclass.h>
#include <dns/rdatatype.h>
#include <dns/result.h>

static isc_mem_t *mctx;
static isc_lex_t *lex;

static isc_lexspecials_t specials;

static void
usage(void) {
	fprintf(stderr, "usage: named-rrchecker [-o origin] [-hpCPT]\n");
	fprintf(stderr, "\t-h: print this help message\n");
	fprintf(stderr, "\t-o origin: set origin to be used when interpeting the record\n");
	fprintf(stderr, "\t-p: print the record in cannonical format\n");
	fprintf(stderr, "\t-C: list the supported class names\n");
	fprintf(stderr, "\t-T: list the supported standard type names\n");
	fprintf(stderr, "\t-P: list the supported private type names\n");
}

int
main(int argc, char *argv[]) {
	isc_token_t token;
	isc_result_t result;
	int c;
	unsigned int options = 0;
	dns_rdatatype_t rdtype;
	dns_rdataclass_t rdclass;
	char text[256*1024];
	char data[64*1024];
	isc_buffer_t tbuf;
	isc_buffer_t dbuf;
	dns_rdata_t rdata = DNS_RDATA_INIT;
	isc_boolean_t doexit = ISC_FALSE;
	isc_boolean_t once = ISC_FALSE;
	isc_boolean_t print = ISC_FALSE;
	isc_boolean_t unknown = ISC_FALSE;
	unsigned int t;
	char *origin = NULL;
	dns_fixedname_t fixed;
	dns_name_t *name = NULL;

	while ((c = isc_commandline_parse(argc, argv, "ho:puCPT")) != -1) {
		switch (c) {
		case '?':
		case 'h':
			if (isc_commandline_option != '?' &&
			    isc_commandline_option != 'h')
				fprintf(stderr, "%s: invalid argument -%c\n",
					argv[0], isc_commandline_option);
			usage();
			exit(1);

		case 'o':
			origin = isc_commandline_argument;
			break;

		case 'p':
			print = ISC_TRUE;
			break;

		case 'u':
			unknown = ISC_TRUE;
			break;

		case 'C':
			for (t = 1; t <= 0xfeffu; t++) {
				if (dns_rdataclass_ismeta(t))
					continue;
				dns_rdataclass_format(t, text, sizeof(text));
				if (strncmp(text, "CLASS", 4) != 0)
					fprintf(stdout, "%s\n", text);
			}
			exit(0);

		case 'P':
			for (t = 0xff00; t <= 0xfffeu; t++) {
				if (dns_rdatatype_ismeta(t))
					continue;
				dns_rdatatype_format(t, text, sizeof(text));
				if (strncmp(text, "TYPE", 4) != 0)
					fprintf(stdout, "%s\n", text);
			}
			doexit = ISC_TRUE;
			break;

		case 'T':
			for (t = 1; t <= 0xfeffu; t++) {
				if (dns_rdatatype_ismeta(t))
					continue;
				dns_rdatatype_format(t, text, sizeof(text));
				if (strncmp(text, "TYPE", 4) != 0)
					fprintf(stdout, "%s\n", text);
			}
			doexit = ISC_TRUE;
			break;
		}
	}
	if (doexit)
		exit(0);

	RUNTIME_CHECK(isc_mem_create(0, 0, &mctx) == ISC_R_SUCCESS);
	RUNTIME_CHECK(isc_lex_create(mctx, 256, &lex) == ISC_R_SUCCESS);

	/*
	 * Set up to lex DNS master file.
	 */

	specials['('] = 1;
	specials[')'] = 1;
	specials['"'] = 1;
	isc_lex_setspecials(lex, specials);
	options = ISC_LEXOPT_EOL;
	isc_lex_setcomments(lex, ISC_LEXCOMMENT_DNSMASTERFILE);

	RUNTIME_CHECK(isc_lex_openstream(lex, stdin) == ISC_R_SUCCESS);

	if (origin != NULL) {
		dns_fixedname_init(&fixed);
		name = dns_fixedname_name(&fixed);
		result = dns_name_fromstring(name, origin, 0, NULL);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "dns_name_fromstring: %s\n",
				dns_result_totext(result));
			fflush(stderr);
			exit(1);
		}
	}

	while ((result = isc_lex_gettoken(lex, options | ISC_LEXOPT_NUMBER,
					  &token)) == ISC_R_SUCCESS) {
		if (token.type == isc_tokentype_eof)
			break;
		if (token.type == isc_tokentype_eol)
			continue;
		if (once) {
			fprintf(stderr, "extra data\n");
			exit(1);
		}
		/*
		 * Get class.
		 */
		if (token.type == isc_tokentype_number) {
			rdclass = (dns_rdataclass_t) token.value.as_ulong;
			if (token.value.as_ulong > 0xffffu) {
				fprintf(stderr, "class value too big %lu\n",
					token.value.as_ulong);
				fflush(stderr);
				exit(1);
			}
			if (dns_rdataclass_ismeta(rdclass)) {
				fprintf(stderr, "class %lu is a meta value\n",
					token.value.as_ulong);
				fflush(stderr);
				exit(1);
			}
		} else if (token.type == isc_tokentype_string) {
			result = dns_rdataclass_fromtext(&rdclass,
					&token.value.as_textregion);
			if (result != ISC_R_SUCCESS) {
				fprintf(stderr, "dns_rdataclass_fromtext: %s\n",
					dns_result_totext(result));
				fflush(stderr);
				exit(1);
			}
			if (dns_rdataclass_ismeta(rdclass)) {
				fprintf(stderr,
					"class %.*s(%d) is a meta value\n",
					(int)token.value.as_textregion.length,
					token.value.as_textregion.base, rdclass);
				fflush(stderr);
				exit(1);
			}
		} else {
			fprintf(stderr, "unexpected token %u\n", token.type);
			exit(1);
		}

		result = isc_lex_gettoken(lex, options | ISC_LEXOPT_NUMBER,
					  &token);
		if (result != ISC_R_SUCCESS)
			break;
		if (token.type == isc_tokentype_eol)
			continue;
		if (token.type == isc_tokentype_eof)
			break;

		/*
		 * Get type.
		 */
		if (token.type == isc_tokentype_number) {
			rdtype = (dns_rdatatype_t) token.value.as_ulong;
			if (token.value.as_ulong > 0xffffu) {
				fprintf(stderr, "type value too big %lu\n",
					token.value.as_ulong);
				exit(1);
			}
			if (dns_rdatatype_ismeta(rdtype)) {
				fprintf(stderr, "type %lu is a meta value\n",
					token.value.as_ulong);
				fflush(stderr);
				exit(1);
			}
		} else if (token.type == isc_tokentype_string) {
			result = dns_rdatatype_fromtext(&rdtype,
					&token.value.as_textregion);
			if (result != ISC_R_SUCCESS) {
				fprintf(stdout, "dns_rdatatype_fromtext: %s\n",
					dns_result_totext(result));
				fflush(stdout);
				exit(1);
			}
			if (dns_rdatatype_ismeta(rdtype)) {
				fprintf(stderr,
					"type %.*s(%d) is a meta value\n",
					(int)token.value.as_textregion.length,
					token.value.as_textregion.base, rdtype);
				fflush(stderr);
				exit(1);
			}
		} else {
			fprintf(stderr, "unexpected token %u\n", token.type);
			exit(1);
		}

		isc_buffer_init(&dbuf, data, sizeof(data));
		result = dns_rdata_fromtext(&rdata, rdclass, rdtype, lex,
					    name, 0, mctx, &dbuf, NULL);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "dns_rdata_fromtext:  %s\n",
				dns_result_totext(result));
			fflush(stderr);
			exit(1);
		}
		once = ISC_TRUE;
	}
	if (result != ISC_R_EOF) {
		fprintf(stderr, "eof not found\n");
		exit(1);
	}
	if (!once) {
		fprintf(stderr, "no records found\n");
		exit(1);
	}

	if (print) {
		isc_buffer_init(&tbuf, text, sizeof(text));
		result = dns_rdataclass_totext(rdclass, &tbuf);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "dns_rdataclass_totext: %s\n",
				dns_result_totext(result));
			fflush(stderr);
			exit(1);
		}
		isc_buffer_putstr(&tbuf, "\t");
		result = dns_rdatatype_totext(rdtype, &tbuf);
		if (result != ISC_R_SUCCESS) {
			fprintf(stderr, "dns_rdatatype_totext: %s\n",
				dns_result_totext(result));
			fflush(stderr);
			exit(1);
		}
		isc_buffer_putstr(&tbuf, "\t");
		result = dns_rdata_totext(&rdata, NULL, &tbuf);
		if (result != ISC_R_SUCCESS)
			fprintf(stderr, "dns_rdata_totext: %s\n",
				dns_result_totext(result));
		else
			fprintf(stdout, "%.*s\n", (int)tbuf.used,
				(char*)tbuf.base);
		fflush(stdout);
	}

	if (unknown) {
		fprintf(stdout, "CLASS%u\tTYPE%u\t\\# %u", rdclass, rdtype,
			rdata.length);
		if (rdata.length != 0) {
			unsigned int i;
			fprintf(stdout, " ");
			for (i = 0; i < rdata.length; i++)
				fprintf(stdout, "%02x", rdata.data[i]);
		}
		fprintf(stdout, "\n");
	}

	isc_lex_close(lex);
	isc_lex_destroy(&lex);
	isc_mem_destroy(&mctx);
	return (0);
}
