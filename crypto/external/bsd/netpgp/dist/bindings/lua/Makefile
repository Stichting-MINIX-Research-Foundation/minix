#PREFIX=/Users/agcrooks
PREFIX=/usr

#LIBDIR=/usr/lib

LIB=luanetpgp
SRCS=glue.c
MKMAN=no
CPPFLAGS+=-g -I${PREFIX}/pkg/include
LDADD+= -lnetpgp
WARNS=4
CLEANFILES+=	a a.sig

.include <bsd.lib.mk>
.include <bsd.own.mk>

LUANETPGPOBJDIR != cd ${.CURDIR} && ${PRINTOBJDIR}

OPSYS!= uname -s

.if ${OPSYS} == "Darwin"
.sinclude <bsd.warns.mk>

lib${LIB}.dylib:
	libtool -dynamic -o ${.TARGET} ${OBJS} ${PREFIX}/pkg/lib/liblua.dylib /usr/lib/libc.dylib ${PREFIX}/pkg/lib/libnetpgp.dylib

t: lib${LIB}.dylib
	cp Makefile a
	./netpgp.lua --sign --detached a
	./netpgp.lua --verify a.sig

.else
t:
	cp Makefile a
	env LD_LIBRARY_PATH=${LUANETPGPOBJDIR}:/lib:/usr/lib:${PREFIX}/lib \
		./netpgp.lua --sign --detached a
	env LD_LIBRARY_PATH=${LUANETPGPOBJDIR}:/lib:/usr/lib:${PREFIX}/lib \
		./netpgp.lua --verify a.sig
.endif
