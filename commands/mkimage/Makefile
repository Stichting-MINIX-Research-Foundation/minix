
NBSD_LIBC:= yes

PROG=	mkimage
SRCS=	mkimage.c
MAN=

CPPFLAGS+= -D_NETBSD_SOURCE

DPADD+=	${LIBELF}
LDADD+=	-lelf

.include <bsd.prog.mk>
