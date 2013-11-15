#	$NetBSD: Makefile,v 1.2 2013/10/30 20:27:40 joerg Exp $

LIB=	lldbPluginSymbolFileDWARF

.include <bsd.init.mk>

.PATH: ${LLDB_SRCDIR}/source/Plugins/SymbolFile/DWARF

SRCS+=	DWARFAbbreviationDeclaration.cpp \
	DWARFCompileUnit.cpp \
	DWARFDataExtractor.cpp \
	DWARFDebugAbbrev.cpp \
	DWARFDebugAranges.cpp \
	DWARFDebugArangeSet.cpp \
	DWARFDebugInfo.cpp \
	DWARFDebugInfoEntry.cpp \
	DWARFDebugLine.cpp \
	DWARFDebugMacinfo.cpp \
	DWARFDebugMacinfoEntry.cpp \
	DWARFDebugPubnames.cpp \
	DWARFDebugPubnamesSet.cpp \
	DWARFDebugRanges.cpp \
	DWARFDeclContext.cpp \
	DWARFDefines.cpp \
	DWARFDIECollection.cpp \
	DWARFFormValue.cpp \
	DWARFLocationDescription.cpp \
	DWARFLocationList.cpp \
	LogChannelDWARF.cpp \
	NameToDIE.cpp \
	SymbolFileDWARF.cpp \
	SymbolFileDWARFDebugMap.cpp \
	UniqueDWARFASTType.cpp

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
