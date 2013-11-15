#	$NetBSD: Makefile,v 1.6 2013/11/28 14:19:59 joerg Exp $

.include <bsd.init.mk>

SUBDIR=	include \
	.WAIT \
	lib \
	.WAIT \
	bin

.if ${MKCLANGRT:Uno} == "yes"
SUBDIR+=	librt
.endif

.include "Makefile.inc"

checkout:
	svn co -r ${CLANG_REVISION} ${SVN_ROOT}/cfe/${SVN_BRANCH} ${CLANG_SRCDIR}
	svn co -r ${COMPILER_RT_REVISION} ${SVN_ROOT}/compiler-rt/${SVN_BRANCH} \
	    ${COMPILER_RT_SRCDIR}
	svn co -r ${LLD_REVISION} ${SVN_ROOT}/lld/${SVN_BRANCH} ${LLD_SRCDIR}
	svn co -r ${LLDB_REVISION} ${SVN_ROOT}/lldb/${SVN_BRANCH} ${LLDB_SRCDIR}
	svn co -r ${LLVM_REVISION} ${SVN_ROOT}/llvm/${SVN_BRANCH} ${LLVM_SRCDIR}

checkout-mclinker:
	if [ -d ${MCLINKER_SRCDIR}/.git ]; then \
		cd ${MCLINKER_SRCDIR}; git pull ${MCLINKER_ROOT} ; \
	else \
		git clone ${MCLINKER_ROOT} ${MCLINKER_SRCDIR}; \
	fi
	cd ${MCLINKER_SRCDIR} && git checkout ${MCLINKER_REVISION}

.include <bsd.subdir.mk>
