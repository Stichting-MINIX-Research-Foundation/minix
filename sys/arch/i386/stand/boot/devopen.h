/*	$NetBSD: devopen.h,v 1.4.64.1 2019/09/13 07:00:13 martin Exp $	*/

extern int boot_biosdev;

void bios2dev(int, daddr_t, char **, int *, int *, const char **);
