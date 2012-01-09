/*	$NetBSD: bootparam.h,v 1.4 2007/11/24 13:20:54 isaki Exp $	*/

int bp_whoami(int);
int bp_getfile(int, char *, struct in_addr *, char *);
