/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that: (1) source distributions retain this entire copyright
 * notice and comment, and (2) distributions including binaries display
 * the following acknowledgement:  ``This product includes software
 * developed by the University of California, Berkeley and its contributors''
 * in the documentation or other materials provided with the distribution
 * and in all advertising materials mentioning features or use of this
 * software. Neither the name of the University nor the names of its
 * contributors may be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)inet_ntoa.c	5.5 (Berkeley) 6/1/90";
#endif /* LIBC_SCCS and not lint */

/*
 * Convert network-format internet address
 * to base 256 d.d.d.d representation.
 */

#include <sys/types.h>
#include <stdio.h>

#include <net/gen/in.h>
#include <net/gen/inet.h>

char *
inet_ntoa(in)
	ipaddr_t in;
{
	static char b[18];
	register u8_t *p;

	p = (u8_t *)&in;
	sprintf(b, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
	return (b);
}
