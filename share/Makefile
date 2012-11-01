#	$NetBSD: Makefile,v 1.32 2010/02/03 15:34:44 roy Exp $
#	from @(#)Makefile	8.1 (Berkeley) 6/5/93

# Missing:  ms

.include <bsd.own.mk>

.if ${MKSHARE} != "no" || \
	make(clean) || make(cleandir) || make(distclean) || make(obj)
SUBDIR= misc mk \
	terminfo zoneinfo
.if ${MKNLS} != "no"
SUBDIR+=i18n locale nls
.endif
.endif

# Speedup stubs for some subtrees that don't need to run these rules
includes-examples includes-man:
	@true

.include <bsd.subdir.mk>
