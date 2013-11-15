#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:10 joerg Exp $

LIB=	MCLinkerFragment

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/Fragment

SRCS=	AlignFragment.cpp \
	FillFragment.cpp \
	Fragment.cpp \
	FragmentLinker.cpp \
	FragmentRef.cpp \
	NullFragment.cpp \
	RegionFragment.cpp \
	Relocation.cpp \
	Stub.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
