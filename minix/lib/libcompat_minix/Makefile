NOGCCERROR=yes
NOCLANGERROR=yes

.include <bsd.own.mk>

LIB=	compat_minix
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

SUBDIR+= pkgconfig include

.include <bsd.lib.mk>
.include <bsd.subdir.mk>
