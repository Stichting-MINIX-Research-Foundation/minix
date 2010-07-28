.include <bsd.own.mk>

BINDIR=	/usr/sbin
# These have to be passed as strings to the c files.
PKG_LOG_DIR=		\"/usr/var/db/pkg\"
PKG_SYCONFDIR=		\"/etc\"
PKG_OPSYS_NAME=		\"Minix\"
PKG_MACHINE_ARCH=   \"${ARCH}\"
PKG_BINDIR=			\"${BINDIR}\"
CPPFLAGS+= -DHAVE_CONFIG_H -D_MINIX -D_POSIX_SOURCE
CPPFLAGS+= -DDEF_LOG_DIR=${PKG_LOG_DIR}
CPPFLAGS+= -DSYSCONFDIR=${PKG_SYCONFDIR}
CPPFLAGS+= -DMACHINE_ARCH=${PKG_MACHINE_ARCH}
CPPFLAGS+= -DOPSYS_NAME=${PKG_OPSYS_NAME}
CPPFLAGS+= -DBINDIR=${PKG_BINDIR}

CPPFLAGS+= -I../ -I../lib

LDFLAGS+= -L../lib
LDADD=		-linstall -lfetch -larchive -lz -lbz2 -lutil
