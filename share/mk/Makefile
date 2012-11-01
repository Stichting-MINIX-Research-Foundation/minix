#	$NetBSD: Makefile,v 1.46 2011/10/07 16:29:40 plunky Exp $
#	@(#)Makefile	8.1 (Berkeley) 6/8/93

NOOBJ=	# defined

.include <bsd.own.mk>

.if ${MKSHARE} != "no"
FILES=	bsd.README bsd.clang-analyze.mk bsd.clean.mk bsd.dep.mk bsd.doc.mk \
	bsd.endian.mk bsd.files.mk \
	bsd.gcc.mk bsd.hostlib.mk bsd.hostprog.mk bsd.inc.mk bsd.info.mk \
	bsd.init.mk bsd.ioconf.mk bsd.kernobj.mk bsd.kinc.mk bsd.klinks.mk \
	bsd.kmodule.mk bsd.lib.mk bsd.links.mk bsd.lua.mk \
	bsd.man.mk bsd.nls.mk \
	bsd.obj.mk bsd.own.mk bsd.prog.mk bsd.rpc.mk bsd.shlib.mk \
	bsd.subdir.mk bsd.sys.mk bsd.test.mk bsd.x11.mk sys.mk

# MINIX-specific files
FILES+= minix.bootprog.mk minix.service.mk minix.gcov.mk

FILESDIR=/usr/share/mk
.endif

FILESMODE=444

.include <bsd.prog.mk>
