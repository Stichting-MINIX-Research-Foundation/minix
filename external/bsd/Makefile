#	$NetBSD: Makefile,v 1.45 2013/11/28 22:34:17 christos Exp $

.include <bsd.own.mk>

SUBDIR= byacc file flex less \
	libarchive mdocml \
	

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
.if (${MKKYUA} != "no")
SUBDIR+= lutok kyua-testers kyua-cli kyua-atf-compat
.endif
.if (${MKLDAP} != "no")
SUBDIR+= openldap
.endif
.if (${MKLIBCXX} != "no")
SUBDIR+= libc++
.endif
.if (${MKLLVM} != "no")
SUBDIR+= llvm
.endif
.if (${MKPCC} != "no")
SUBDIR+= pcc
.endif

SUBDIR+= nvi

.include <bsd.subdir.mk>
