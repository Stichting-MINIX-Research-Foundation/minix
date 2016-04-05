#	$NetBSD: Makefile,v 1.79 2013/03/01 18:25:27 joerg Exp $

.include <bsd.own.mk>

SUBDIR=	altq arch dev fs \
	net net80211 netatalk netinet netinet6 \
        netmpls \
	sys ufs uvm

.if !defined(__MINIX)
# interrupt implementation depends on the kernel within the port
#.if (${MACHINE} != "evbppc")
.if make(obj) || make(cleandir) || ${MKKMOD} != "no"
SUBDIR+=modules
.endif
#.endif
.endif # !defined(__MINIX)

# LSC FIXME: Remove this test as soon as we have imported RUMP
.if !defined(__MINIX)
.if make(includes) || make(obj) || make(cleandir)
SUBDIR+= rump
.endif
.endif # !defined(__MINIX)

.include <bsd.kinc.mk>
