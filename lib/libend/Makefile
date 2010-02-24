# Makefile for libend

.include "Makefile.inc"

LIB=end

SRCS=	\
	edata.s \
	em_end.s \
	end.s \
	etext.s

.if ${COMPILER_TYPE} == "gnu"
.include "${.CURDIR}/gnu/Makefile.inc"
.endif

.include <minix.lib.mk>
