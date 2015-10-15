#	$NetBSD: Makefile,v 1.8 2014/05/31 22:55:17 mrg Exp $

MYOBJS=		${G_OBJC_OBJS} ${G_C_AND_OBJC_OBJS} 
PROG=		cc1obj
SRCS=		${MYOBJS:S,objc/,,:S,c-family/,,:S,c/,,:Nlibcpp.a:.o=.c} main.c ${PROG}-checksum.c

CPPFLAGS+=	-I${DIST}/gcc/objc
CPPFLAGS.prefix.c+=	-DPREFIX=\"/usr\"

.include "../Makefile.backend"
.include "../Makefile.backtrace"
.include "../Makefile.common"
.include "../Makefile.common-target"
.include "../Makefile.libcpp"
.include "../Makefile.libdecnumber"
.include "../Makefile.libiberty"

CHECKSUM_OBJS=	${BACKENDOBJ}/libbackend.a \
		main.o \
		${LIBBACKTRACEOBJ}/libbacktrace.a \
		${COMMONOBJ}/libcommon.a \
		${COMMONTARGETOBJ}/libcommon-target.a \
		${LIBCPPOBJ}/libcpp.a \
		${LIBDECNUMBEROBJ}/libdecnumber.a \
		${LIBIBERTYOBJ}/libiberty.a \
		${MYOBJS:S,objc/,,:S,c-family/,,:S,c/,,} \
		checksum-options

.include "../Makefile.checksum"

# XXX check this out
# LDFLAGS+=	-rdynamic

# VER_CPPFLAGS from Makefile.inc
CPPFLAGS.c-cppbuiltin.c=	${VER_CPPFLAGS}
c-cppbuiltin.c: ${GCCARCH}/defs.mk

COPTS.c-ada-spec.c+=			-Wno-stack-protector
COPTS.c-aux-info.c+=			-Wno-stack-protector
COPTS.c-common.c+=			-Wno-stack-protector
COPTS.c-cppbuiltin.c+=			-Wno-stack-protector
COPTS.c-format.c+=			-Wno-stack-protector
COPTS.c-lex.c+=				-Wno-stack-protector
COPTS.c-ppoutput.c+=			-Wno-stack-protector
COPTS.c-pragma.c+=			-Wno-stack-protector
COPTS.c-typeck.c+=			-Wno-stack-protector
COPTS.objc-act.c+=			-Wno-stack-protector
COPTS.objc-gnu-runtime-abi-01.c+=	-Wno-stack-protector
COPTS.objc-next-runtime-abi-01.c+=	-Wno-stack-protector

.if defined(__MINIX)
LDADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} -lz -lm
DPADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} ${LIBZ} ${LIBM}
.else
LDADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} -lintl -lz -lm
DPADD+=	${LIBMPC} ${LIBMPFR} ${LIBGMP} ${LIBINTL} ${LIBZ} ${LIBM}
.endif # defined(__MINIX)

.PATH: ${DIST}/gcc/objc ${DIST}/gcc/c ${DIST}/gcc/c-family
.PATH: ${DIST}/gcc ${DIST}/gcc/config
.PATH: ${G_out_file:H}
