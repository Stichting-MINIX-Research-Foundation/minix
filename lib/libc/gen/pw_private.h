/*	$NetBSD: pw_private.h,v 1.2 2003/07/26 19:24:43 salo Exp $	*/

/*
 * Written by Jason R. Thorpe <thorpej@NetBSD.org>, June 26, 1998.
 * Public domain.
 */

int	__pw_scan __P((char *bp, struct passwd *pw, int *flags));
