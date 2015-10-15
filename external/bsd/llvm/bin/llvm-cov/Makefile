#	$NetBSD: Makefile,v 1.4 2015/01/29 20:41:34 joerg Exp $

PROG_CXX=	llvm-cov
NOMAN=		yes

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/llvm-cov

SRCS=	gcov.cpp \
	llvm-cov.cpp \
	CodeCoverage.cpp \
	CoverageFilters.cpp \
	CoverageReport.cpp \
	CoverageSummary.cpp \
	CoverageSummaryInfo.cpp \
	SourceCoverageView.cpp \
	TestingSupport.cpp

LLVM_LIBS+= \
	ProfileData \
	Object \
	IR \
	Support

.include "${.PARSEDIR}/../../link.mk"

.include <bsd.prog.mk>
