/*	$NetBSD: ext.h,v 1.2 2011/11/09 19:10:10 christos Exp $	*/

/* borrowed from ../../bin/csh/extern.h */
void prusage(FILE *, struct rusage *, struct rusage *, struct timespec *,
        struct timespec *);
