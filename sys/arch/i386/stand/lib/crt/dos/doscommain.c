/*	$NetBSD: doscommain.c,v 1.6 2008/12/14 18:46:33 christos Exp $	*/

/*
 * Copyright (c) 1996
 *	Matthias Drochner.  All rights reserved.
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
 *
 */

/* argument line processing for DOS .COM programs */

#include <lib/libsa/stand.h>

/* The Program Segment Prefix */

static struct psp{
	char mist1[0x2c];
	short envseg;
	char mist2[0x80-2-0x2c];
	char cmdlen;
	char cmd[127];
} *PSP = (struct psp*)0;

static char* argv[64]; /* theor max */

static int whitespace(char);

static int
whitespace(char c)
{
	if ((c == '\0') || (c == ' ') || (c == '\t')
	    || (c == '\r') || (c == '\n'))
		return (1);
	return 0;
}

enum state {skipping, doing_arg, doing_long_arg};

/* build argv/argc, start real main() */
int doscommain(void);
extern int main(int, char**);

int
doscommain(void)
{
	int argc, i;
	enum state s;

	argv[0] = "???"; /* we don't know */
	argc = 1;
	s = skipping;

	for (i = 0; i < PSP->cmdlen; i++){

		if (whitespace(PSP->cmd[i])) {
			if (s == doing_arg) {
				/* end of argument word */
				PSP->cmd[i] = '\0';
				s = skipping;
			}
			continue;
		}

		if (PSP->cmd[i] == '"') {
			/* start or end long arg
			 * (end only if next char is whitespace)
			 *  XXX but '" ' cannot be in argument
			 */
			switch (s) {
			case skipping:
				/* next char begins new argument word */
				argv[argc++] = &PSP->cmd[i + 1];
				s = doing_long_arg;
				break;
			case doing_long_arg:
				if (whitespace(PSP->cmd[i + 1])) {
					PSP->cmd[i] = '\0';
					s = skipping;
				}
			case doing_arg:
				/* ignore in the middle of arguments */
			default:
				break;
			}
			continue;
		}

		/* all other characters */
		if (s == skipping) {
			/* begin new argument word */
			argv[argc++] = &PSP->cmd[i];
			s = doing_arg;
		}
	}
	if (s != skipping)
		PSP->cmd[i] = '\0'; /* to be sure */

	/* start real main() */
	return main(argc, argv);
}
