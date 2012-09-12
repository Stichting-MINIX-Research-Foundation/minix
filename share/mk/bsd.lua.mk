#	$NetBSD: bsd.lua.mk,v 1.4 2011/10/16 00:45:09 mbalmer Exp $
#
# Build rules and definitions for Lua modules

#
# Variables used
#
# LUA_VERSION	currently installed version of Lua
# LUA_LIBDIR	${LIBDIR}/lua/${LUA_VERSION}
#
# LUA_MODULES	list of Lua modules to build/installi
# LUA_DPLIBS	shared library dependencies as per LIBDPLIBS
#
# LUA_SRCS.mod	sources for each module (by default: "${mod:S/./_/g}.lua")
#
# DPADD		additional dependencies for building modules
# DPADD.mod	additional dependencies for a specific module
#
#
# HAVE_LUAC	if defined, .lua source files will be compiled with ${LUAC}
#		and installed as precompiled chunks for faster loading. Note
#		that the luac file format is not yet standardised and may be
#		subject to change.
#
# LUAC		the luac compiler (by default: /usr/bin/luac)
#
#
# Notes:
#
# currently make(depend) and make(tags) do not support .lua sources; We
# add Lua sources to DPSRCS when HAVE_LUAC is defined and other language
# sources to SRCS for <bsd.dep.mk>.
#
# other language support for other than C is incomplete
#
# C language sources are passed though lint, when MKLINT != "no"
#
# The Lua binary searches /usr/share/lua/5.1/ at this time and we could
# install .lua modules there which would mean slightly less duplication
# in compat builds. However, MKSHARE=no would prevent such modules from
# being installed so we just install everything under /usr/lib/lua/5.1/
#

.if !defined(_BSD_LUA_MK_)
_BSD_LUA_MK_=1

.include <bsd.init.mk>
.include <bsd.shlib.mk>
.include <bsd.gcc.mk>

##
##### Basic targets
realinstall:	.PHONY lua-install
realall:	.PHONY lua-all
lint:		.PHONY lua-lint

lua-install:	.PHONY

lua-all:	.PHONY

lua-lint:	.PHONY

CLEANFILES+= a.out [Ee]rrs mklog core *.core

##
##### Global variables
LUA_VERSION?=	5.1
LUA_LIBDIR?=	${LIBDIR}/lua/${LUA_VERSION}
LUAC?=		/usr/bin/luac

##
##### Build rules

# XX should these always be on?
CFLAGS+=	-fPIC -DPIC

.SUFFIXES:	.lua .luac
.lua.luac:
	${_MKTARGET_COMPILE}
	${LUAC} -o ${.TARGET} ${.IMPSRC}

##
##### Libraries that modules may depend upon.
.for _lib _dir in ${LUA_DPLIBS}
.if !defined(LIBDO.${_lib})
LIBDO.${_lib}!=	cd "${_dir}" && ${PRINTOBJDIR}
.MAKEOVERRIDES+=LIBDO.${_lib}
.endif
LDADD+=-L${LIBDO.${_lib}} -l${_lib}
DPADD+=${LIBDO.${_lib}}/lib${_lib}.so
.endfor

##
##### Lua Modules
.for _M in ${LUA_MODULES}
LUA_SRCS.${_M}?=${_M:S/./_/g}.lua
LUA_DEST.${_M}=${LUA_LIBDIR}${_M:S/./\//g:S/^/\//:H}

.if !empty(LUA_SRCS.${_M}:M*.lua)
.if ${LUA_SRCS.${_M}:[\#]} > 1
.error Module "${_M}" has too many source files
.endif
.if defined(HAVE_LUAC)
##
## The module has Lua source and needs to be compiled
LUA_TARG.${_M}=${_M:S/./_/g}.luac
LUA_NAME.${_M}=${_M:S/./\//g:T}.luac
CLEANFILES+=${LUA_TARG.${_M}}
DPSRCS+=${LUA_SRCS.${_M}}

.NOPATH:		${LUA_TARG.${_M}}
lua-all:		${LUA_TARG.${_M}}
${LUA_TARG.${_M}}:	${LUA_SRCS.${_M}} ${DPADD} ${DPADD.${_M}}
.else
##
## The module has Lua source and can be installed directly
LUA_TARG.${_M}=${LUA_SRCS.${_M}}
LUA_NAME.${_M}=${_M:S/./\//g:T}.lua
.endif
.else
##
## The module has other language source and we must build ${_M}.so
LUA_OBJS.${_M}=${LUA_SRCS.${_M}:N*.lua:R:S/$/.o/g}
LUA_LOBJ.${_M}=${LUA_SRCS.${_M}:M*.c:.c=.ln}
LUA_TARG.${_M}=${_M:S/./_/g}.so
LUA_NAME.${_M}=${_M:S/./\//g:T}.so
CLEANFILES+=${LUA_OBJS.${_M}} ${LUA_LOBJ.${_M}} ${LUA_TARG.${_M}}
DPSRCS+=${LUA_SRCS.${_M}}
SRCS+=${LUA_SRCS.${_M}}

.NOPATH:		${LUA_OBJS.${_M}} ${LUA_LOBJ.${_M}} ${LUA_TARG.${_M}}
.if ${MKLINT} != "no"
${LUA_TARG.${_M}}:	${LUA_LOBJ.${_M}}
.endif
lua-lint:		${LUA_LOBJ.${_M}}
lua-all:		${LUA_TARG.${_M}}
${LUA_TARG.${_M}}:	${LUA_OBJS.${_M}} ${DPADD} ${DPADD.${_M}}
	${_MKTARGET_BUILD}
	rm -f ${.TARGET}
	${CC} -Wl,--warn-shared-textrel \
	    -Wl,-x -shared ${LUA_OBJS.${_M}} \
	    -Wl,-soname,${LUA_NAME.${_M}} -o ${.TARGET} \
	    ${LDADD} ${LDADD.${_M}} ${LDFLAGS} ${LDFLAGS.${_M}}

.endif

##
## module install rules
lua-install:		${DESTDIR}${LUA_DEST.${_M}}/${LUA_NAME.${_M}}
${DESTDIR}${LUA_DEST.${_M}}/${LUA_NAME.${_M}}! ${LUA_TARG.${_M}}
	${_MKTARGET_INSTALL}
	${INSTALL_FILE} -o ${LIBOWN} -g ${LIBGRP} -m ${LIBMODE}	\
	    ${.ALLSRC} ${.TARGET}

.endfor
##
##### end of modules

.include <bsd.clean.mk>
.include <bsd.dep.mk>
.include <bsd.inc.mk>
.include <bsd.obj.mk>
.include <bsd.sys.mk>
.endif	# ! defined(_BSD_LUA_MK_)
