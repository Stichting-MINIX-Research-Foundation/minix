#	$NetBSD: Makefile,v 1.7 2012/02/23 11:05:02 nonaka Exp $

# build GCC's libgomp, so that -fopenmp works.

# XXX
NOLINT=	# defined

.include <bsd.own.mk>

.if !defined(NO_LIBGOMP)	# {

DIST=	${NETBSDSRCDIR}/external/gpl3/gcc/dist

LIB=	gomp
SRCS= \
	alloc.c \
	barrier.c \
	critical.c \
	env.c \
	error.c \
	iter.c \
	iter_ull.c \
	loop.c \
	loop_ull.c \
	ordered.c \
	parallel.c \
	sections.c \
	single.c \
	task.c \
	team.c \
	work.c \
	lock.c \
	mutex.c \
	proc.c \
	sem.c \
	bar.c \
	ptrlock.c \
	time.c \
	fortran.c \
	affinity.c

GOMP_MACHINE_ARCH?=	${MACHINE_ARCH}
ARCHDIR=		${.CURDIR}/arch/${GOMP_MACHINE_ARCH}

CPPFLAGS+= \
	-DHAVE_CONFIG_H \
	-I${DIST}/libgomp \
	-I${DIST}/libgomp/config/posix \
	-I${DIST}/libgcc \
	-I${ARCHDIR}
CFLAGS+=-pthread

CWARNFLAGS.clang+=	-Wno-conversion

# libgomp.so.1.0 is in GCC 4.5.
SHLIB_MAJOR=	1
SHLIB_MINOR=	0

LDFLAGS+=	-Wl,--version-script,${DIST}/libgomp/libgomp.map

.PATH:	${ARCHDIR} ${DIST}/libgomp ${DIST}/libgomp/config/posix

.if ${GOMP_MACHINE_ARCH} == ${MACHINE_ARCH}
# This is where GCC looks for it.
FILES=		libgomp.spec
FILESDIR=	${LIBDIR}

INCS=		omp.h
INCSDIR=	/usr/include/gcc-4.5
.endif

.else			# } else {
libinstall::
.endif			# }

.include <bsd.lib.mk>

COPTS+=	-Wno-stack-protector
