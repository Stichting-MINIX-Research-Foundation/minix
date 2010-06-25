#	$NetBSD: Makefile,v 1.50 2010/04/22 19:15:23 sjg Exp $
#	@(#)Makefile	5.2 (Berkeley) 12/28/90

PROG=	make
SRCS=	arch.c buf.c compat.c cond.c dir.c for.c hash.c job.c main.c \
	make.c parse.c str.c suff.c targ.c trace.c var.c util.c
SRCS+=  strlist.c
SRCS+=  make_malloc.c
SRCS+=	lstAppend.c lstAtEnd.c lstAtFront.c lstClose.c lstConcat.c \
	lstDatum.c lstDeQueue.c lstDestroy.c lstDupl.c lstEnQueue.c \
	lstFind.c lstFindFrom.c lstFirst.c lstForEach.c lstForEachFrom.c \
	lstInit.c lstInsert.c lstIsAtEnd.c lstIsEmpty.c lstLast.c \
	lstMember.c lstNext.c lstOpen.c lstRemove.c lstReplace.c lstSucc.c
SRCS+=	lstPrev.c


# For MINIX
CPPFLAGS+=	-DHAVE_SETENV -DHAVE_STRERROR -DHAVE_STRDUP \
		-DHAVE_STRFTIME -DHAVE_VSNPRINTF -DUSE_SELECT
MACHINE=${ARCH}
MACHINE_ARCH=${ARCH}
CPPFLAGS+= -DTARGET_MACHINE=\"${MACHINE}\" \
	-DTARGET_MACHINE_ARCH=\"${MACHINE_ARCH}\" \
	-DMAKE_MACHINE=\"${MACHINE}\"


 .PATH:	${.CURDIR}/lst.lib

# .if make(obj) || make(clean)
# SUBDIR+= unit-tests
# .endif

.include <bsd.prog.mk>
.include <bsd.subdir.mk>

# A simple unit-test driver to help catch regressions
accept test:
	cd ${.CURDIR}/unit-tests && ${.MAKE} -r -m / TEST_MAKE=${TEST_MAKE:U${.OBJDIR}/${PROG:T}} ${.TARGET}
