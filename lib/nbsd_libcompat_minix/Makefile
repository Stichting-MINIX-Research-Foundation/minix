.include <bsd.own.mk>

LIB=	compat_minix
CPPFLAGS+= -D_MINIX_COMPAT
MACHINE_ARCH= ${ARCH}

.PATH: 	${.CURDIR}

# std_err()
SRCS+= stderr.c

# V8 regexp (incompatible with BSD regexp)
SRCS+= v8regerror.c   v8regexp.c   v8regsub.c

# Configfile Library.
SRCS+= configfile.c

# ACK a.out format.
SRCS+= nlist.c

# Minix 'mtab' handling functions.
#
# These should be changed when we switch to
# NetBSD's 'mtab' format.
SRCS+= mtab.c

# fttyslot(fd), a Minix-specific extension
SRCS+= fttyslot.c

# cuserid()
# Now considered "compat" feature in NetBSD.
SRCS+= cuserid.c

.include "include/Makefile.inc"

.include <bsd.own.mk>
SUBDIR+= pkgconfig
.include <bsd.subdir.mk>

.include <bsd.lib.mk>
