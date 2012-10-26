#	$NetBSD: Makefile,v 1.61 2012/02/07 19:13:31 joerg Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/4/93

.include <bsd.own.mk>

SUBDIR= \
	\
	ld.elf_so

.if defined(__MINIX)
SUBDIR+= makewhatis
.else
.if ${MKMAKEMANDB} == "no"
SUBDIR+= getNAME makewhatis
.endif
.endif # defined(__MINIX)

.include <bsd.subdir.mk>
