# $NetBSD: Makefile,v 1.25 2015/05/01 14:17:56 christos Exp $

.include <bsd.own.mk>

TESTSDIR=	${TESTSBASE}/lib/libc/stdlib

TESTS_C+=	t_abs
TESTS_C+=	t_atoi
TESTS_C+=	t_div
TESTS_C+=	t_getenv
# LSC: Unsupported on MINIX
#TESTS_C+=	t_getenv_thread
TESTS_C+=	t_exit
TESTS_C+=	t_hsearch
TESTS_C+=	t_mi_vector_hash
# LSC: Unsupported on MINIX
#TESTS_C+=	t_posix_memalign
TESTS_C+=	t_random
TESTS_C+=	t_strtod
TESTS_C+=	t_strtol
TESTS_C+=	t_strtoi
TESTS_C+=	t_system

TESTS_SH+=	t_atexit
TESTS_SH+=	t_getopt

MKMAN=no

BINDIR=		${TESTSDIR}

PROGS+=		h_atexit
PROGS+=		h_getopt h_getopt_long

.if ${MACHINE_CPU} == "aarch64" || ${MACHINE_CPU} == "arm" \
    || ${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64" \
    || ${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64"
CPPFLAGS.t_strtod.c+=	-DHAVE_FENV
.endif

LDADD.t_strtod=			-lm
DPADD.t_strtod+=		${LIBM}

LDADD.t_getenv_thread=		-lpthread

.include <bsd.test.mk>
