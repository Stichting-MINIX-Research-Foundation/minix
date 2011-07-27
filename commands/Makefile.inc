# Any of the commands can force the new libc being used by 
# setting NEED_NBSDLIBC
.if defined(NEED_NBSDLIBC)
.include <minix.newlibc.mk>
.endif

.if (${NBSD_LIBC} != "no")
CPPFLAGS+= -D_MINIX -D_NETBSD_SOURCE -D_MINIX_COMPAT
LDADD+= -lminlib -lcompat_minix -lasyn -lterminfo
.else
CPPFLAGS+= -D_MINIX -D_POSIX_SOURCE
.endif

BINDIR?=/usr/bin
