#	$NetBSD: Makefile,v 1.7 2015/07/23 07:36:54 mrg Exp $

.include <bsd.own.mk>

# These are used by the 'includes' phase,
# and can't assume that ${DESTDIR}${X11INCDIR} has been populated.

#SUBDIR+= makestrs .WAIT

# The rest of the tools can rely upon 'includes'

# makekeys is now done src/tools
SUBDIR+=bdftopcf fc-cache ucs2any
SUBDIR+=mkfontscale .WAIT xkbcomp

.if defined(__MINIX)
SUBDIR+=makekeys
.endif # defined(__MINIX)

.include <bsd.buildinstall.mk>
.include <bsd.subdir.mk>
