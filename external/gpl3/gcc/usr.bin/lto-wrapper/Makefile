#	$NetBSD: Makefile,v 1.1 2015/01/07 02:02:44 christos Exp $

NOMAN=1
PROG=		lto-wrapper
SRCS=		lto-wrapper.c
CPPFLAGS+=	-DPREFIX=\"/usr\"

.include "../Makefile.backend"
.include "../Makefile.backtrace"
.include "../Makefile.common-target"
.include "../Makefile.libcpp"

DPADD+=                 ${LIBIBERTYOBJ}/libiberty.a
LDADD+=                 ${LIBIBERTYOBJ}/libiberty.a


# VER_CPPFLAGS from Makefile.inc

.if !defined(__MINIX)
LDADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} -lintl -lz -lm
DPADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} ${LIBINTL} ${LIBZ} ${LIBM}
.else
LDADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} -lz -lm
DPADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} ${LIBZ} ${LIBM}
.endif # !defined(__MINIX)

.PATH: ${DIST}/gcc ${DIST}/gcc/c ${DIST}/gcc/c-family ${DIST}/gcc/config
.PATH: ${G_out_file:H}
