#PREFIX=/Users/agcrooks
PREFIX=/usr

#LIBDIR=/usr/lib

LIB=luabozohttpd
SRCS=glue.c
MKMAN=no
CPPFLAGS+=-g -I${PREFIX}/pkg/include
LDADD+= -lbozohttpd
WARNS=4
CLEANFILES+=	a a.sig

.include <bsd.lib.mk>
.include <bsd.own.mk>

LUABOZOOBJDIR != cd ${.CURDIR} && ${PRINTOBJDIR}

OPSYS!= uname -s

.if ${OPSYS} == "Darwin"
.sinclude <bsd.warns.mk>

lib${LIB}.dylib:
	libtool -dynamic -o ${.TARGET} ${OBJS} ${PREFIX}/pkg/lib/liblua.dylib /usr/lib/libc.dylib ${PREFIX}/pkg/lib/libbozohttpd.dylib

t: lib${LIB}.dylib
	cp Makefile a
	./bozo.lua --sign --detached a
	./bozo.lua --verify a.sig

.else
t:
	cp Makefile a
	env LD_LIBRARY_PATH=${LUABOZOOBJDIR}:/lib:/usr/lib:${PREFIX}/lib \
		./bozo.lua --sign --detached a
	env LD_LIBRARY_PATH=${LUABOZOOBJDIR}:/lib:/usr/lib:${PREFIX}/lib \
		./bozo.lua --verify a.sig
.endif
