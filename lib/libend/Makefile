# Makefile for libend

.include "Makefile.inc"

LIB=end

SRCS=	\
	edata.S \
	em_end.S \
	end.S \
	etext.S

.if ${COMPILER_TYPE} == "gnu"
.include "${.CURDIR}/gnu/Makefile.inc"
.endif

.include <bsd.lib.mk>
