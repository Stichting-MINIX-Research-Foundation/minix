# Makefile for Virtual File System (VFS)
.include <bsd.own.mk>

PROG=	vfs
SRCS=	main.c open.c read.c write.c pipe.c dmap.c \
	path.c device.c mount.c link.c exec.c \
	filedes.c stadir.c protect.c time.c \
	lock.c misc.c utility.c select.c table.c \
	vnode.c vmnt.c request.c \
	tll.c comm.c worker.c coredump.c \
	bdev.c cdev.c sdev.c smap.c socket.c

.if ${MKCOVERAGE} != "no"
SRCS+=  gcov.c
CPPFLAGS+= -DUSE_COVERAGE
.endif

.if defined(__MINIX)
#LSC: -Wno-maybe-uninitialized while compiling with -DNDEBUG -O3
CWARNFLAGS.gcc+=	-Wno-maybe-uninitialized
.endif # defined(__MINIX)

CFLAGS+= -Wall -Wextra -Wno-sign-compare -Werror
DPADD+=	${LIBSYS} ${LIBTIMERS} ${LIBEXEC}
LDADD+=	-lsys -ltimers -lexec -lmthread

.include <minix.service.mk>
