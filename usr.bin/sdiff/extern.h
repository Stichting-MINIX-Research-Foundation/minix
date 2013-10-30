/*	$NetBSD: extern.h,v 1.1 2007/02/18 22:13:42 rmind Exp $	*/
/*	$OpenBSD: extern.h,v 1.4 2006/05/25 03:20:32 ray Exp $ */

/*
 * Written by Raymond Lai <ray@cyth.net>.
 * Public domain.
 */

extern FILE		*outfile;	/* file to save changes to */
extern const char	*tmpdir;

int eparse(const char *, const char *, const char *);
