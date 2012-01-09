/*	$NetBSD: checkpasswd.c,v 1.9 2011/01/06 02:45:13 jakllsch Exp $	*/

/*-
 * Copyright (c) 1993
 *	The Regents of the University of California.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)gets.c	8.1 (Berkeley) 6/11/93
 */

#ifdef _STANDALONE
#include <lib/libkern/libkern.h>
#else
#include <string.h>
#endif

#include "stand.h"

char *
getpass(const char *prompt)
{
	int c;
	char *lp;
	static char buf[128]; /* == _PASSWORD_LEN */

	printf("%s", prompt);

	for (lp = buf;;) {
		switch (c = getchar() & 0177) {
		case '\n':
		case '\r':
			*lp = '\0';
			putchar('\n');
			return buf;
		case '\b':
		case '\177':
			if (lp > buf) {
				lp--;
				putchar('\b');
				putchar(' ');
				putchar('\b');
			}
			break;
#if HASH_ERASE
		case '#':
			if (lp > buf)
				--lp;
			break;
#endif
		case 'r'&037: {
			char *p;

			putchar('\n');
			for (p = buf; p < lp; ++p)
				putchar('*');
			break;
		}
#if AT_ERASE
		case '@':
#endif
		case 'u'&037:
		case 'w'&037:
			lp = buf;
			putchar('\n');
			break;
		default:
			*lp++ = c;
			putchar('*');
			break;
		}
	}
	/*NOTREACHED*/
}

#include <sys/md5.h>

char bootpasswd[16] = {'\0'}; /* into data segment! */

int
checkpasswd(void)
{

	return check_password(bootpasswd);
}

int
check_password(const char *password)
{
	int i;
	char *passwd;
	MD5_CTX md5ctx;
	char pwdigest[16];

	for (i = 0; i < 16; i++)
		if (password[i])
			break;
	if (i == 16)
		return 1; /* no password set */

	for (i = 0; i < 3; i++) {
		passwd = getpass("Password: ");
		MD5Init(&md5ctx);
		MD5Update(&md5ctx, passwd, strlen(passwd));
		MD5Final(pwdigest, &md5ctx);
		if (memcmp(pwdigest, password, 16) == 0)
			return 1;
	}

	/* failed */
	return 0;
}
