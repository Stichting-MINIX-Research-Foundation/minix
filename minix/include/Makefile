# Doing a make includes builds /usr/include

NOOBJ=		# defined

INCS+=	lib.h libutil.h varargs.h configfile.h

.include <bsd.own.mk>

INCSDIR=	/usr/include

SUBDIR+=	arch
SUBDIR+=	ddekit
SUBDIR+=	libdde
SUBDIR+=	minix
SUBDIR+=	net
SUBDIR+=	sys

.include <bsd.prog.mk>
.include <bsd.subdir.mk>
