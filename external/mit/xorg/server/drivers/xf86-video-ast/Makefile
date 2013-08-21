#	$NetBSD: Makefile,v 1.4 2011/08/11 23:15:39 joerg Exp $

DRIVER=		xf86-video-ast
DRIVER_NAME=	ast_drv

SRCS=		ast_2dtool.c ast_accel.c ast_cursor.c ast_driver.c
SRCS+=		ast_mode.c ast_tool.c ast_vgatool.c

CWARNFLAGS.clang+=	-Wno-format

.include "../Makefile.xf86-driver"
