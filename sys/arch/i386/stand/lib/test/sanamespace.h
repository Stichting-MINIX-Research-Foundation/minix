/*	$NetBSD: sanamespace.h,v 1.2 2014/03/26 17:58:57 christos Exp $	*/

/* take back the namespace mangling done by "Makefile.satest" */

#undef main
#undef exit
#undef free
#undef open
#undef close
#undef read
#undef write
#undef ioctl
#undef lseek
#undef printf
#undef vprintf
#undef putchar
#undef gets
#undef strerror
#undef errno
