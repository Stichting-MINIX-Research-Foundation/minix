/* $NetBSD: example_client.c,v 1.4 2011/02/12 23:21:33 christos Exp $ */

/* Copyright (c) 2010 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Mateusz Kocielski.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.	IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__RCSID("$NetBSD: example_client.c,v 1.4 2011/02/12 23:21:33 christos Exp $");

#include <err.h>
#include <limits.h>
#include <saslc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void
print_help(void)
{

	printf("usage: [-hl] {-m mech_name}\n");
	printf("-h - help\n");
	printf("-l - mechanisms list\n");
	printf("-m - use mech_name mechanism\n");
}

static void
list_mechanisms(void)
{

	printf("available mechanisms:\n");
	printf("ANONYMOUS, CRAM-MD5, DIGEST-MD5, GSSAPI, EXTERNAL, LOGIN, "
	    "PLAIN\n");
}

static char *
nextline(char *buf, size_t len, FILE *fp)
{
	char *p;

	if (fgets(buf, len, fp) == NULL)
		return NULL;

	if ((p = strchr(buf, '\n')) != NULL)
		*p = '\0';

	return buf;
}

int
main(int argc, char **argv)
{
	int opt, n, cont;
	char *mechanism = NULL;
	saslc_t *ctx;
	saslc_sess_t *sess;
	char input[LINE_MAX];
	char *option, *var;
	char *output;
	static char empty[] = "";
	size_t input_len, output_len;

	while ((opt = getopt(argc, argv, "hm:l")) != -1) {
		switch (opt) {
		case 'm':
			/* mechanism */
			mechanism = optarg;
			break;
		case 'l':
			/* list mechanisms */
			list_mechanisms();
			return EXIT_SUCCESS;
		case 'h':
		default:
			/* ??? */
			print_help();
			return EXIT_FAILURE;
		}
	}

	if (mechanism == NULL) {
		printf("mechanism: ");
		if (nextline(input, sizeof(input), stdin) == NULL)
			goto eof;
		mechanism = input;
	}

	ctx = saslc_alloc();

	if (saslc_init(ctx, NULL, NULL) < 0)
		goto error;

	if ((sess = saslc_sess_init(ctx, mechanism, NULL)) == NULL)
		goto error;

	/* reading properties */
	if (nextline(input, sizeof(input), stdin) == NULL)
		goto eof;
	n = atoi(input);

	while (n--) {
		if (nextline(input, sizeof(input), stdin) == NULL)
			goto eof;
		var = strchr(input, ' ');
		if (var != NULL)
			*var++ = '\0';
		else
			var = empty;
		option = input;
		if (saslc_sess_setprop(sess, option, var) < 0)
			goto error;
	}

	printf("session:\n");

	for (;;) {
		if (nextline(input, sizeof(input), stdin) == NULL)
			break;
		input_len = strlen(input);
		cont = saslc_sess_cont(sess, input, input_len, (void **)&output,
		    &output_len);
		if (cont < 0)
			goto error_sess;
		printf("%s\n", output == NULL ? "empty line" : output);
		if (cont == 0)
			break;
	}

	saslc_sess_end(sess);
	if (saslc_end(ctx) < 0)
		goto error;

	return 0;
 eof:
	err(EXIT_FAILURE, "Unexpected EOF");
 error:
	errx(EXIT_FAILURE, "%s", saslc_strerror(ctx));
 error_sess:
	errx(EXIT_FAILURE, "%s", saslc_sess_strerror(sess));
}
