/*	$NetBSD: pw_private.h,v 1.3 2012/03/20 16:36:05 matt Exp $	*/

/*
 * Written by Jason R. Thorpe <thorpej@NetBSD.org>, June 26, 1998.
 * Public domain.
 */

int	__pw_scan(char *bp, struct passwd *pw, int *flags);
