#	$NetBSD: Makefile,v 1.104 2014/02/24 07:23:41 skrll Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/5/93

# MINIX:
MAN=	\
	\
	\
	\
	rc.8 rc.subr.8 \
	\
	
#MLINKS+=MAKEDEV.8 makedev.8
#MLINKS+=MAKEDEV.local.8 makedev.local.8
#MLINKS+=compat_svr4.8 compat_sco.8
#MLINKS+=compat_svr4.8 compat_solaris.8
#MLINKS+=compat_svr4.8 compat_svr4_32.8
#MLINKS+=compat_svr4.8 compat_xenix.8
#MLINKS+=compat_netbsd32.8 netbsd32.8
#MLINKS+=nis.8 yp.8
MLINKS+=rc.8 rc.d.8
MLINKS+=rc.8 rc.local.8
MLINKS+=rc.8 rc.shutdown.8

.if !defined(__MINIX)
SUBDIR= man8.acorn26 man8.acorn32 man8.alpha man8.amiga man8.atari \
	man8.cobalt man8.dreamcast man8.emips man8.evbarm \
	man8.hp300 man8.hpcarm man8.hpcmips man8.hpcsh man8.hppa \
	man8.mac68k man8.macppc \
	man8.mvme68k man8.next68k man8.pmax man8.prep man8.sandpoint \
	man8.sgimips man8.sparc man8.sparc64 man8.sun2 man8.sun3 \
	man8.vax man8.x68k man8.x86

# create MAKEDEV.8 from ../../etc/MAKEDEV.tmpl
makedevs:
	cd ${.CURDIR} && ${HOST_SH} MAKEDEV2manpage.sh
.endif # !defined(__MINIX)

.include <bsd.man.mk>
.include <bsd.subdir.mk>
