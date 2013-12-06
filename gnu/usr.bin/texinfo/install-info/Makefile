#	$NetBSD: Makefile,v 1.10 2004/02/09 06:52:34 lukem Exp $

PROG=		install-info
SRCS=		install-info.c

.include <bsd.prog.mk>

.PATH: ${IDIST}/util ${IDIST}/doc

infodir-meta:
.if (${MKUNPRIVED} != "no" && ${MKINFO} != "no")
	echo "./usr/share/info/dir type=file mode=0644 uname=${BINOWN} gname=${BINGRP}" \
	    | ${METALOG.add}
.endif
