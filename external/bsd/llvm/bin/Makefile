#	$NetBSD: Makefile,v 1.14 2014/03/04 21:05:05 joerg Exp $

.include <bsd.own.mk>

SUBDIR=	clang \
	clang-tblgen \
	tblgen

.if ${MKLLD} != "no"
SUBDIR+=	\
	lld
.endif

.if ${MKLLDB} != "no"
SUBDIR+=	\
	lldb
.endif

.if ${MKMCLINKER} != "no"
SUBDIR+=	\
	llvm-mcld
.endif

.if defined(LLVM_DEVELOPER)
SUBDIR+= \
	bugpoint \
	clang-format \
	llc \
	lli \
	llvm-ar \
	llvm-as \
	llvm-bcanalyzer \
	llvm-cov \
	llvm-diff \
	llvm-dis \
	llvm-dwarfdump \
	llvm-extract \
	llvm-link \
	llvm-mc \
	llvm-nm \
	llvm-objdump \
	llvm-profdata \
	llvm-readobj \
	llvm-size \
	llvm-symbolizer \
	macho-dumpx \
	opt
.endif

.include <bsd.subdir.mk>
