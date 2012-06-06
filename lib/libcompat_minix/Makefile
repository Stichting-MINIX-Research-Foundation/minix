.include <bsd.own.mk>

LIB=	compat_minix
CPPFLAGS+= -D_MINIX_COMPAT
USE_SHLIBDIR= yes

.PATH: 	${.CURDIR}

# std_err()
SRCS+= stderr.c

# Configfile Library.
SRCS+= configfile.c

# Minix 'mtab' handling functions.
#
# These should be changed when we switch to
# NetBSD's 'mtab' format.
SRCS+= mtab.c

.include "include/Makefile.inc"

.include <bsd.own.mk>
SUBDIR+= pkgconfig
.include <bsd.subdir.mk>

.include <bsd.lib.mk>
