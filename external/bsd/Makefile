#	$NetBSD: Makefile,v 1.43 2013/04/28 00:14:14 joerg Exp $

.include <bsd.own.mk>

SUBDIR= byacc file flex mdocml \
	libarchive
.if (${MKATF} != "no")
SUBDIR+= atf
.endif
.if (${MKCRYPTO} != "no")
SUBDIR+= pkg_install
.endif
# IP Filter
.if (${MKIPFILTER} != "no")
SUBDIR+=ipf
.endif
.if (${MKISCSI} != "no")
SUBDIR+= iscsi 
.endif
.if (defined(__MINIX) && ${MKGCCCMDS} == "yes")
.if (${MKKYUA} != "no")
SUBDIR+= lutok kyua-testers kyua-cli kyua-atf-compat
.endif
.endif # (defined(__MINIX) && ${MKGCCCMDS} == "yes")
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
