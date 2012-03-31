NOOBJ=	# defined

.include <bsd.own.mk>

.if ${MKSHARE} != "no"
FILES=	bsd.dep.mk bsd.files.mk \
	bsd.inc.mk bsd.info.mk \
	bsd.init.mk bsd.kinc.mk bsd.klinks.mk bsd.lib.mk \
	bsd.links.mk bsd.man.mk bsd.obj.mk bsd.own.mk \
	bsd.prog.mk bsd.subdir.mk bsd.sys.mk bsd.doc.mk \
	bsd.shlib.mk sys.mk

# MINIX-specific files
FILES+= minix.bootprog.mk minix.service.mk \
	minix.gcc.mk minix.gcov.mk pkgsrchooks.mk 

FILESDIR=/usr/share/mk
.endif

FILESMODE=444

.include <bsd.prog.mk>
