#	$NetBSD: Makefile,v 1.1 2013/02/27 21:25:10 joerg Exp $

LIB=	MCLinkerMC

.include <bsd.init.mk>

.PATH: ${MCLINKER_SRCDIR}/lib/MC

SRCS=	Attribute.cpp \
	AttributeSet.cpp \
	CommandAction.cpp \
	ContextFactory.cpp \
	FileAction.cpp \
	InputAction.cpp \
	InputBuilder.cpp \
	InputFactory.cpp \
	MCLDDirectory.cpp \
	MCLDInput.cpp \
	SearchDirs.cpp \
	SymbolCategory.cpp \
	ZOption.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
