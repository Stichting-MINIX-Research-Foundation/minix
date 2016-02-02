#	$NetBSD: Makefile,v 1.139 2014/08/08 19:38:47 apb Exp $

.include <bsd.own.mk>

.if defined(__MINIX)
SUBDIR+=	texinfo
.else
SUBDIR+=	bc
SUBDIR+=	c89 c99
SUBDIR+=	dc diffutils
SUBDIR+=	rcs send-pr texinfo

.if ${MKBSDGREP} == "no"
SUBDIR+=	grep
.endif

.if ${MKGROFF} != "no"
SUBDIR+=	groff
.endif

SUBDIR+=	gettext
.endif # defined(__MINIX)

.include <bsd.subdir.mk>
