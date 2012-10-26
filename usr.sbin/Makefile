#	$NetBSD: Makefile,v 1.266 2012/08/08 14:13:46 christos Exp $
#	from: @(#)Makefile	5.20 (Berkeley) 6/12/93

.include <bsd.own.mk>

SUBDIR= \
	chroot \
	installboot \
	mtree \
	pwd_mkdb postinstall \
	user \
	vipw \
	zic

# LSC MINIX Specific
SUBDIR+= mkfs.mfs \
	mkproto

.if !defined(__MINIX)
.if ${MKMAKEMANDB} != "no"
SUBDIR+= makemandb
.endif
.endif # !defined(__MINIX)

.if (${MKYP} != "no")
SUBDIR+= rpc.yppasswdd ypbind yppoll ypserv ypset
.endif

.if (${MKRUMP} != "no")
SUBDIR+= puffs
.endif

.if ${TOOLCHAIN_MISSING} != "no"
SUBDIR+= mdsetimage
.endif

.if (${MKINET6} != "no")
SUBDIR+=faithd ifmcstat ndp rip6query rtsold
.endif
.if (${USE_INET6} != "no")
SUBDIR+=mld6query route6d rtadvd traceroute6 
.endif

.if !defined(__MINIX)
# ATM PVC
SUBDIR+=pvcsif pvctxctl
.endif

.if (${MKCRYPTO} != "no")
SUBDIR+= racoon racoonctl
.endif

# NPF
.if (${MKNPF} != "no")
SUBDIR+=npf
.endif

# PF
.if (${MKPF} != "no")
SUBDIR+=pf
.endif

.include <bsd.subdir.mk>
