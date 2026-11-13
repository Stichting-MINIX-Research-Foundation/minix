/*	$NetBSD: prompt.c,v 1.6 2020/01/25 10:09:46 jmcneill Exp $	*/

/*
 * Copyright (c) 1996, 1997
 * 	Matthias Drochner.  All rights reserved.
 * Copyright (c) 1996, 1997
 * 	Perry E. Metzger.  All rights reserved.
 * Copyright (c) 1997
 *	Jason R. Thorpe.  All rights reserved
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
 *    must display the following acknowledgements:
 *	This product includes software developed for the NetBSD Project
 *	by Matthias Drochner.
 *	This product includes software developed for the NetBSD Project
 *	by Perry E. Metzger.
 * 4. The names of the authors may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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

#include "efiboot.h"

#include <lib/libsa/net.h>
#include <sys/syslimits.h>

#define	POLL_FREQ	10

char *
gettrailer(char *arg)
{
	char *options;

	for (options = arg; *options; options++) {
		switch (*options) {
		case ' ':
		case '\t':
			*options++ = '\0';
			break;
		default:
			continue;
		}
		break;
	}
	if (*options == '\0')
		return options;

	/* trim leading blanks/tabs */
	while (*options == ' ' || *options == '\t')
		options++;

	return options;
}

char
awaitkey(int timeout, int tell)
{
	int i = timeout * POLL_FREQ;
	int last_secs = -1, secs;
	int last_len = -1, n;
	char buf[32];
	char c = 0;

	for (;;) {
		if (tell) {
			int len;

			secs = (i + POLL_FREQ - 1) / POLL_FREQ;
			if (secs != last_secs) {
				if (last_len != -1) {
					char *p = buf;
					for (n = 0; n < last_len; n++)
						*p++ = '\b';
					*p = '\0';
					printf("%s", buf);
				}
				len = snprintf(buf, sizeof(buf), "%d seconds. ", (i + POLL_FREQ - 1) / POLL_FREQ);
				if (len > 0 && len < sizeof(buf))
					printf("%s", buf);
				last_len = len;
				last_secs = secs;
			}
		}
		if (ischar()) {
			c = getchar();
			if (c == 0)
				c = -1;
			goto out;
		}
		if (--i > 0) {
			efi_delay(1000000 / POLL_FREQ);
		} else {
			break;
		}
	}

out:
	if (tell) {
		if (last_len != -1) {
			char *p = buf;
			for (n = 0; n < last_len; n++)
				*p++ = '\b';
			*p = '\0';
			printf("%s", buf);
		}
		printf("0 seconds.     \n");
	}

	return c;
}

void
docommand(char *arg)
{
	char *options;
	int i;

	options = gettrailer(arg);

	for (i = 0; commands[i].c_name != NULL; i++) {
		if (strcmp(arg, commands[i].c_name) == 0) {
			(*commands[i].c_fn)(options);
			return;
		}
	}

	printf("unknown command\n");
	command_help(NULL);
}

__dead void
bootprompt(void)
{
	char input[LINE_MAX];

	for (;;) {
		char *c = input;

		input[0] = '\0';
		printf("> ");
		kgets(input, sizeof(input));

		/*
		 * Skip leading whitespace.
		 */
		while (*c == ' ')
			c++;
		if (*c)
			docommand(c);
	}
}
