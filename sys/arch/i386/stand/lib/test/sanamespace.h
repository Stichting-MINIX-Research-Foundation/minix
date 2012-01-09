/*	$NetBSD: sanamespace.h,v 1.1 1998/05/15 17:07:16 drochner Exp $	*/

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
#undef sprintf
#undef vprintf
#undef putchar
#undef gets
#undef strerror
#undef errno
