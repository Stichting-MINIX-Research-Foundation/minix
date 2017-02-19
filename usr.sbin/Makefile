#	$NetBSD: Makefile,v 1.271 2015/08/17 06:42:46 knakahara Exp $
#	from: @(#)Makefile	5.20 (Berkeley) 6/12/93

.include <bsd.own.mk>

SUBDIR= arp \
	\
	chroot \
	dev_mkdb \
	\
	\
	\
	\
	i2cscan inetd installboot \
	\
	\
	link \
	makefs \
	mtree \
	\
	\
	pwd_mkdb postinstall \
	\
	\
	rdate \
	\
	service services_mkdb \
	\
	syslogd \
	traceroute \
	unlink user \
	vipw vnconfig \
	\
	zdump zic

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
SUBDIR+= ndp
.endif
.if (${USE_INET6} != "no")
SUBDIR+= rtadvd traceroute6
.endif

.if !defined(__MINIX)
# ATM PVC
SUBDIR+=pvcsif pvctxctl

.if (${MKCRYPTO} != "no")
SUBDIR+= racoon racoonctl
.endif
.endif # !defined(__MINIX)

# NPF
.if (${MKNPF} != "no")
SUBDIR+=npf
.endif

# PF
.if (${MKPF} != "no")
SUBDIR+=pf
.endif

.include <bsd.subdir.mk>
