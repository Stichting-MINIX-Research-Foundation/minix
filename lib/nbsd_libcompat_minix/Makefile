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

# Minix legacy passwd format
# These should be removed when we switch to
# NetBSD's 'passwd' db-based format.
SRCS+= getpwent.c

# fttyslot(fd), a Minix-specific extension
SRCS+= fttyslot.c

# cuserid()
# Now considered "compat" feature in NetBSD.
SRCS+= cuserid.c

# XXX: hack
# user_from_uid(), uid_from_user()
# group_from_gid(), gid_from_group()
SRCS+= passwd.c

.include "include/Makefile.inc"

.include <bsd.own.mk>
SUBDIR+= pkgconfig
.include <bsd.subdir.mk>

.include <bsd.lib.mk>
