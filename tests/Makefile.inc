# $NetBSD: Makefile.inc,v 1.2 2011/09/16 16:30:18 joerg Exp $
WARNS ?= 4
CWARNFLAGS+= -Wno-missing-noreturn

.if defined(__MINIX)
LDSTATIC= -dynamic
NOGCCERROR=yes
NOCLANGERROR=yes
.endif # defined(__MINIX)
