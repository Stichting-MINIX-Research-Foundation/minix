#	$NetBSD: Makefile.inc,v 1.11 2012/01/20 16:31:30 joerg Exp $

.PATH: ${.CURDIR}/nls

SRCS+=	catclose.c catgets.c catopen.c 
MAN+=	catclose.3 catgets.3 catopen.3

# indirect reference stubs, to be removed soon.
SRCS+=	_catclose.c _catgets.c _catopen.c

CPPFLAGS.catopen.c+=	-I${LIBCDIR}/citrus

