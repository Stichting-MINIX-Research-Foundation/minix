#	$NetBSD: Makefile,v 1.1 2013/09/29 12:12:00 joerg Exp $

LIB=	LLVMgold

.include <bsd.init.mk>

.PATH: ${LLVM_SRCDIR}/tools/gold

SRCS+=	gold-plugin.cpp

# LSC: ensure the dynamic module is built
LOADABLE_MODULE:=	1
LINK_COMPONENTS := support
LINK_LIBS_IN_SHARED := 1
SHARED_LIBRARY := 1
EXPORTED_SYMBOL_FILE = $(PROJ_SRC_DIR)/gold.exports

#  LIBS += -lLTO

CPPFLAGS+=	-I ${NETBSDSRCDIR}/external/gpl3/binutils/dist/include

.if defined(HOSTLIB)
.include <bsd.hostlib.mk>
.else
.include <bsd.lib.mk>
.endif
