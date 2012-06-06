/*	$NetBSD: underscore.c,v 1.1 2009/08/18 20:22:08 skrll Exp $	*/

#ifdef __ELF__
int prepends_underscore = 0;
#else
int prepends_underscore = 1;
#endif
