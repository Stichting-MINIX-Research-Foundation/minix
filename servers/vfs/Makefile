# Makefile for Virtual File System (VFS)
.include <bsd.own.mk>

PROG=	vfs
SRCS=	main.c open.c read.c write.c pipe.c dmap.c \
	path.c device.c mount.c link.c exec.c \
	filedes.c stadir.c protect.c time.c \
	lock.c misc.c utility.c select.c table.c \
	vnode.c vmnt.c request.c fscall.c

.if ${MKCOVERAGE} != "no"
SRCS+=	gcov.c
CPPFLAGS+= -DUSE_COVERAGE
.endif

DPADD+=	${LIBSYS} ${LIBTIMERS} ${LIBEXEC}
LDADD+=	-lsys -ltimers -lexec

MAN=

BINDIR?= /usr/sbin
INSTALLFLAGS+=	-S 16k

.include <minix.bootprog.mk>
