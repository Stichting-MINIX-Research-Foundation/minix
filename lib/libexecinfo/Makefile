# $NetBSD: Makefile,v 1.8 2014/10/24 22:42:16 joerg Exp $

.include <bsd.own.mk>

LIBEXECINFO_MACHINE_ARCH?= ${MACHINE_ARCH}
USE_UNWIND=yes
WARNS?=4
INCSDIR=/usr/include
INCS=execinfo.h

.if ${HAVE_LIBGCC_EH} == "yes"
INCS+=	unwind.h
.endif

#CPPFLAGS+='-D__RCSID(a)=' -D_GNU_SOURCE '-D__printflike(a,b)='
#CPPFLAGS+=-I/usr/include/libelf
#COPTS+=-std=gnu99

LIBDPLIBS+= elf	${NETBSDSRCDIR}/external/bsd/elftoolchain/lib/libelf

LIB=execinfo
SRCS=symtab.c backtrace.c
MAN= backtrace.3

.if ${USE_UNWIND} == "yes"
SRCS+=unwind.c
.if !empty(LIBEXECINFO_MACHINE_ARCH:Mearm*)
SRCS+=unwind_arm_ehabi_stub.c
.endif
.else
SRCS+=builtin.c
.endif

MLINKS+= backtrace.3 backtrace_symbols.3
MLINKS+= backtrace.3 backtrace_symbols_fmt.3
MLINKS+= backtrace.3 backtrace_symbols_fd.3
MLINKS+= backtrace.3 backtrace_symbols_fd_fmt.3

.include <bsd.lib.mk>
