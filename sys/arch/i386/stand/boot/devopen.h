/*	$NetBSD: devopen.h,v 1.4 2010/12/24 20:40:42 jakllsch Exp $	*/

extern int boot_biosdev;

void bios2dev(int, daddr_t, char **, int *, int *);
