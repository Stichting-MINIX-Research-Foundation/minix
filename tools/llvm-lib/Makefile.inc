#	$NetBSD: Makefile.inc,v 1.1 2011/02/06 01:14:12 joerg Exp $

.include "${.PARSEDIR}/../llvm/Makefile.inc"

HOSTLIB=	${LIB}

.include "${LLVM_TOPLEVEL}/lib/Makefile.inc"
.include "${LLVM_TOPLEVEL}/lib/${.CURDIR:T}/Makefile"

HOST_CPPFLAGS:=	${HOST_CPPFLAGS:N-Wp,-iremap,*}
