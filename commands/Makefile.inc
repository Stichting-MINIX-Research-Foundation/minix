.if (${NBSD_LIBC} != "no")
CPPFLAGS+= -D_MINIX -D_NETBSD_SOURCE -D_MINIX_COMPAT
LDADD+= -lminlib -lcompat_minix -lasyn -lterminfo
.else
CPPFLAGS+= -D_MINIX -D_POSIX_SOURCE
.endif

BINDIR?=/usr/bin
