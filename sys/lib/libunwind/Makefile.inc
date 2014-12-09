#	$NetBSD: Makefile.inc,v 1.5 2014/03/19 16:46:23 joerg Exp $

.PATH:	${NETBSDSRCDIR}/sys/lib/libunwind

SRCS+=	libunwind.cxx \
	unwind_registers.S

.if defined(__MINIX)
SRCS+=	aeabi_unwind.c

# do not install unwind.h this for minix
#COPTS.libunwind.cxx+=	"-I${NETBSDSRCDIR}/sys/lib/libunwind"
.endif

INCS+=	unwind.h

.if ${HAVE_GCC:U48} == "4"
CLEANFILES+=	cstdint
DPSRCS+=	cstdint

cstdint:
	echo '#include <stdint.h>' > ${.TARGET}

COPTS.libunwind.cxx+=	"-D__builtin_unreachable()=abort()"
.else
COPTS.libunwind.cxx+=	${${ACTIVE_CXX} == "gcc":? -std=c++0x : -std=c++11 }
.endif
COPTS.libunwind.cxx+=	-funwind-tables -fno-rtti -fno-exceptions -fvisibility=hidden -Wno-old-style-cast
