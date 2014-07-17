.include <bsd.own.mk>

USE_SHLIBDIR= yes

LIB=	gcc_s

LDFLAGS+=       -nodefaultlibs
LDFLAGS+=       -Wl,--version-script=${.CURDIR}/libgcc_s_empty.map

SRCS+= empty.c

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
