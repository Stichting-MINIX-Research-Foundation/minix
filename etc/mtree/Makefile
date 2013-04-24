#	$NetBSD: Makefile,v 1.22 2011/09/10 17:02:46 apb Exp $

.include <bsd.own.mk>

.if ${MKX11} != "no"
. if ${X11FLAVOUR} == "Xorg"
EXTRA_DIST_FILES=	${.CURDIR}/NetBSD.dist.Xorg
. else
EXTRA_DIST_FILES=	${.CURDIR}/NetBSD.dist.XFree86
. endif
.endif

.if defined(__MINIX)
.if ${MKGCCCMDS} == "yes"
EXTRA_DIST_FILES+=	${.CURDIR}/Minix.gcccmds
.endif
.endif # defined(__MINIX)

# XXX these are only used by compat currently, but they could be used
# by something else; this may need to be fixed properly in the future.
.if ${MKCOMPAT} != "no"
.if exists(NetBSD.dist.${MACHINE_ARCH})
EXTRA_DIST_FILES+=	${.CURDIR}/NetBSD.dist.${MACHINE_ARCH}
.endif
.endif

.if ${MKATF} != "no"
EXTRA_DIST_FILES+=	${.CURDIR}/NetBSD.dist.tests
.endif

.if ${MKEXTSRC} != "no"
EXTRA_DIST_FILES+=	${.CURDIR}/NetBSD.dist.extsrc
.endif

NetBSD.dist:	NetBSD.dist.tmp
	cmp -s NetBSD.dist.tmp NetBSD.dist || { \
		echo "Updating NetBSD.dist"; \
		mv NetBSD.dist.tmp NetBSD.dist; \
	}
NetBSD.dist.tmp::
	${TOOL_CAT} ${.CURDIR}/NetBSD.dist.base ${EXTRA_DIST_FILES} > \
	    ${.TARGET}

CONFIGFILES=	NetBSD.dist special
FILESDIR=	/etc/mtree

# distrib-dirs --
#	Populate $DESTDIR with directories needed by NetBSD
#
.if ${MKUNPRIVED} == "no"
TOOL_MTREE.unpriv=
.else
TOOL_MTREE.unpriv=	-W
.endif

# postinstall(8) invokes this target to produce the right
# /etc/mtree/NetBSD.dist content without duplicating logic from
# the Makefile.
#
emit_dist_file:
	cat ${.CURDIR}/NetBSD.dist.base ${EXTRA_DIST_FILES}

distrib-dirs: .PHONY check_DESTDIR NetBSD.dist
.if !defined(DISTRIBUTION_DONE)						# {
# Create DESTDIR using HOST_INSTALL_DIR, not INSTALL_DIR, because
# INSTALL_DIR would want to write to the metalog, and it can't do that
# if the metalog is inside DESTDIR but DESTDIR doesn't yet exist.
	${HOST_INSTALL_DIR} -m 755 ${DESTDIR}
# Invoke mtree to create the directories listed in NetBSD.dist;
# then invoke mtree again to register those directories in the metalog.
	${TOOL_MTREE} -def ${.OBJDIR}/NetBSD.dist -N ${.CURDIR}/.. \
	    -p ${DESTDIR}/ -U ${TOOL_MTREE.unpriv}
.if ${MKUNPRIVED} != "no"						# {
	${TOOL_MTREE} -def ${.OBJDIR}/NetBSD.dist -N ${.CURDIR}/.. \
	    -p ${DESTDIR}/ -C -k all | \
	    ${TOOL_AWK} '/ optional/ {next} // {print}' | ${METALOG.add}
.endif	# MKUNPRIVED							# }
.endif	# DISTRIBUTION_DONE						# }

CLEANFILES+=	NetBSD.dist NetBSD.dist.tmp

.include <bsd.prog.mk>
