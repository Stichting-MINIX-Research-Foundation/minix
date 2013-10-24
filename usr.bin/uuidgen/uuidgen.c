/*	$NetBSD: uuidgen.c,v 1.4 2011/09/16 15:39:30 joerg Exp $	*/

/*-
 * Copyright (c) 2004 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe.
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

/*
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
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

#include <sys/cdefs.h>
__RCSID("$NetBSD: uuidgen.c,v 1.4 2011/09/16 15:39:30 joerg Exp $");

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <uuid.h>

__dead static void
usage(void)
{

	(void)fprintf(stderr, "usage: %s [-1s] [-n count] [-o filename]\n",
	    getprogname());
	exit(1);
}

int
main(int argc, char *argv[])
{
	FILE *fp;
	uuid_t *store, *uuid;
	char *p;
	int ch, count, i, iterate, c_struct;

	count = -1;	/* no count yet */
	fp = stdout;	/* default output file */
	iterate = 0;	/* not one at a time */
	c_struct = 0;	/* not as a C structure */

	while ((ch = getopt(argc, argv, "1n:o:s")) != -1) {
		switch (ch) {
		case '1':
			iterate = 1;
			break;
		case 'n':
			if (count > 0)
				usage();
			count = strtol(optarg, &p, 10);
			if (*p != 0 || count < 1)
				usage();
			break;
		case 'o':
			if (fp != stdout)
				errx(1, "multiple output files not allowed");
			fp = fopen(optarg, "w");
			if (fp == NULL)
				err(1, "fopen");
			break;
		case 's':
			c_struct = 1;
			break;
		default:
			usage();
		}
	}
	argv += optind;
	argc -= optind;

	if (argc)
		usage();

	if (count == -1)
		count = 1;

	store = (uuid_t*)malloc(sizeof(uuid_t) * count);
	if (store == NULL)
		err(1, "malloc()");

	if (!iterate) {
		/* Get them all in a single batch */
		if (uuidgen(store, count) != 0)
			err(1, "uuidgen()");
	} else {
		uuid = store;
		for (i = 0; i < count; i++) {
			if (uuidgen(uuid++, 1) != 0)
				err(1, "uuidgen()");
		}
	}

	uuid = store;
	while (count--) {
		uuid_to_string(uuid++, &p, NULL);
		if (c_struct) {
			fprintf(fp, "= { /* %s */\n", p);	/* } */
			/*
			 * Chunk up the string for processing:
			 *
			 *	aaaaaaaa-bbbb-cccc-dddd-0123456789ab
			 *
			 * We output it like so:
			 *
			 * = { \/\* aaaaaaaa-bbbb-cccc-ddee-0123456789ab \*\/
			 *	0xaaaaaaaa,
			 *	0xbbbb,
			 *	0xcccc,
			 *	0xdd,
			 *	0xee,
			 *	{ 0x01, 0x23, 0x45, 0x67, 0x89, 0xab }
			 * };
			 */
			p[8] = '\0';	/* aaaaaaaa */
			p[13] = '\0';	/* bbbb */
			p[18] = '\0';	/* cccc */
			p[23] = '\0';	/* dddd */
			fprintf(fp, "\t0x%s,\n", p);
			fprintf(fp, "\t0x%s,\n", &p[9]);
			fprintf(fp, "\t0x%s,\n", &p[14]);
			fprintf(fp, "\t0x%c%c,\n", p[19], p[20]);
			fprintf(fp, "\t0x%c%c,\n", p[21], p[22]);
			fprintf(fp, "\t{ 0x%c%c, 0x%c%c, 0x%c%c, 0x%c%c, "
					"0x%c%c, 0x%c%c }\n",
				p[24], p[25], p[26], p[27],
				p[28], p[29], p[30], p[31],
				p[32], p[33], p[34], p[35]);
	/* { */		fprintf(fp, "};\n");
		} else
			fprintf(fp, "%s\n", p);
		free(p);
	}

	free(store);
	if (fp != stdout)
		fclose(fp);
	return (0);
}
