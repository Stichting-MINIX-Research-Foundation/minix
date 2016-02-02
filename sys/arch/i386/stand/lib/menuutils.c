/*	$NetBSD: menuutils.c,v 1.4 2014/04/06 19:11:26 jakllsch Exp $	*/

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

#include <lib/libkern/libkern.h>
#include <lib/libsa/stand.h>

#include "libi386.h"

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
bootmenu(void)
#if defined(__minix)
{
	prompt(0);
	while(1); /* This should never return. */
}

/* Derived from libsa gets(). */
void
editline(char *buf, size_t size, char *input)
{
	int c, i, pos, len = 0;

	/* If an initial input has been given, copy and print this first. */
	if (input != NULL) {
		while (*input && len < size - 1)
			putchar(buf[len++] = *input++);
	}
	pos = len;

	for (;;) {
		c = getchar_ex();
		switch (c & 0177) {
		case '\0':
			switch (c) {
			case 0x4b00: /* Left arrow: move cursor to left. */
				if (pos > 0) {
					putchar('\b');
					pos--;
				}
				break;
			case 0x4d00: /* Right arrow: move cursor to right. */
				if (pos < len) putchar(buf[pos++]);
				break;
			}
			break;
		case 'b' & 037: /* Ctrl+B: move cursor to left. */
			if (pos > 0) {
				putchar('\b');
				pos--;
			}
			break;
		case 'f' & 037: /* Ctrl+F: move cursor to right. */
			if (pos < len) putchar(buf[pos++]);
			break;
		case 'a' & 037: /* Ctrl+A: move cursor to start of line. */
			for ( ; pos > 0; pos--) putchar('\b');
			break;
		case 'e' & 037: /* Ctrl+E: move cursor to end of line. */
			for ( ; pos < len; pos++) putchar(buf[pos]);
			break;
		case '\n': /* Enter: return line. */
		case '\r':
			for ( ; pos < len; pos++) putchar(buf[pos]);
			buf[len] = '\0';
			putchar('\n');
			return;
#if HASH_ERASE
		case '#':
#endif
		case '\b': /* Backspace: erase character before cursor. */
		case '\177':
			if (pos > 0) {
				pos--;
				len--;
				putchar('\b');
				for (i = pos; i < len; i++)
					putchar(buf[i] = buf[i + 1]);
				putchar(' ');
				for (i = pos; i < len; i++) putchar('\b');
				putchar('\b');
			}
			break;
		case 'r' & 037: /* Ctrl+R: reprint line. */
			putchar('\n');
			for (i = 0; i < len; i++) putchar(buf[i]);
			for (i = len; i > pos; i--) putchar('\b');
			break;
#if AT_ERASE
		case '@':
#endif
		case 'u' & 037: /* Ctrl+U: clear entire line. */
		case 'w' & 037:
			for ( ; pos > 0; pos--) putchar('\b');
			for ( ; pos < len; pos++) putchar(' ');
			for ( ; pos > 0; pos--) putchar('\b');
			len = 0;
			break;
		case '\a': /* Ctrl+G: sound bell but do not store character. */
			putchar(c);
			break;
		case '\t': /* Tab: convert to single space. */
			c = ' ';
			/*FALLTHROUGH*/
		default: /* Insert character at cursor position. */
			if (len < size - 1) {
				for (i = len; i > pos; i--)
					buf[i] = buf[i - 1];
				buf[pos] = c;
				pos++;
				len++;
				putchar(c);
				for (i = pos; i < len; i++) putchar(buf[i]);
				for (i = pos; i < len; i++) putchar('\b');
			} else {
				putchar('\a');
			}
			break;
		}
	}
	/*NOTREACHED*/
}

void
prompt(int allowreturn)
#endif /* defined(__minix) */
{
	char input[80];

	for (;;) {
		char *c = input;

		input[0] = '\0';
		printf("> ");
#if !defined(__minix)
		gets(input);
#else
		editline(input, sizeof(input), NULL);
#endif /* !defined(__minix) */

		/*
		 * Skip leading whitespace.
		 */
		while (*c == ' ')
			c++;
#if defined(__minix)
		if (allowreturn && !strcmp(c, "menu"))
			break;
#endif /* defined(__minix) */
		if (*c)
			docommand(c);
	}
}
