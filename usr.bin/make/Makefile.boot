#	$NetBSD: Makefile.boot,v 1.20 2011/03/26 21:42:12 dholland Exp $
#
# a very simple makefile...
#
# You only want to use this if you aren't running NetBSD.
#
# modify MACHINE and MACHINE_ARCH as appropriate for your target architecture
#
CC=gcc -O -g

.c.o:
	${CC} ${CFLAGS} -c $< -o $@

MACHINE=i386
MACHINE_ARCH=i386
# tested on HP-UX 10.20
#MAKE_MACHINE=hp700
#MAKE_MACHINE_ARCH=hppa
CFLAGS= -DTARGET_MACHINE=\"${MACHINE}\" \
	-DTARGET_MACHINE_ARCH=\"${MACHINE_ARCH}\" \
	-DMAKE_MACHINE=\"${MACHINE}\"
LIBS=

OBJ=arch.o buf.o compat.o cond.o dir.o for.o hash.o job.o main.o make.o \
    make_malloc.o parse.o str.o strlist.o suff.o targ.o trace.o var.o util.o

LIBOBJ= lst.lib/lstAppend.o lst.lib/lstAtEnd.o lst.lib/lstAtFront.o \
	lst.lib/lstClose.o lst.lib/lstConcat.o lst.lib/lstDatum.o \
	lst.lib/lstDeQueue.o lst.lib/lstDestroy.o lst.lib/lstDupl.o \
	lst.lib/lstEnQueue.o lst.lib/lstFind.o lst.lib/lstFindFrom.o \
	lst.lib/lstFirst.o lst.lib/lstForEach.o lst.lib/lstForEachFrom.o \
	lst.lib/lstInit.o lst.lib/lstInsert.o lst.lib/lstIsAtEnd.o \
	lst.lib/lstIsEmpty.o lst.lib/lstLast.o lst.lib/lstMember.o \
	lst.lib/lstNext.o lst.lib/lstOpen.o lst.lib/lstRemove.o \
	lst.lib/lstReplace.o lst.lib/lstSucc.o lst.lib/lstPrev.o

bmake: ${OBJ} ${LIBOBJ}
#	@echo 'make of make and make.0 started.'
	${CC} ${CFLAGS} ${OBJ} ${LIBOBJ} -o bmake ${LIBS}
	@ls -l $@
#	nroff -h -man make.1 > make.0
#	@echo 'make of make and make.0 completed.'

clean:
	rm -f ${OBJ} ${LIBOBJ} ${PORTOBJ} bmake
