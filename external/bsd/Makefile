#	$NetBSD: Makefile,v 1.33 2012/10/08 01:45:11 jkoshy Exp $

.include <bsd.own.mk>

SUBDIR= byacc file flex mdocml \
	libarchive
.if (${MKATF} != "no")
SUBDIR+= atf
.endif
.if (${MKCRYPTO} != "no")
SUBDIR+= pkg_install ../../crypto/external/bsd
.endif
# IP Filter
.if (${MKIPFILTER} != "no")
SUBDIR+=ipf
.endif
.if (${MKISCSI} != "no")
SUBDIR+= iscsi 
.endif
.if (${MKLDAP} != "no")
SUBDIR+= openldap
.endif
.if (${MKLLVM} != "no")
SUBDIR+= llvm
.endif
.if (${MKPCC} != "no")
SUBDIR+= pcc
.endif

.include <bsd.subdir.mk>
