# $NetBSD: mkvars.mk,v 1.6 2011/11/02 14:42:43 he Exp $

MKEXTRAVARS= \
	MACHINE \
	MACHINE_ARCH \
	MACHINE_CPU \
	HAVE_GCC \
	HAVE_GDB \
	HAS_SSP \
	OBJECT_FMT \
	TOOLCHAIN_MISSING \
	EXTSRCS \
	MKMANZ \
	MKBFD \
	MKCOMPAT \
	MKCOMPATMODULES \
	MKDYNAMICROOT \
	MKMANPAGES \
	MKXORG \
	X11FLAVOR \
	USE_INET6 \
	USE_KERBEROS \
	USE_LDAP \
	USE_YP \
	NETBSDSRCDIR \
	MAKEVERBOSE

#####

.include <bsd.own.mk>
.include <bsd.sys.mk>

.if (${MKMAN} == "no" || empty(MANINSTALL:Mmaninstall))
MKMANPAGES=no
.else
MKMANPAGES=yes
.endif

.if ${MKX11} != "no"
. if ${X11FLAVOUR} == "Xorg"
MKXORG:=yes
MKX11:=no
. else
MKXORG:=no
. endif
.endif

#####

mkvars: mkvarsyesno mkextravars mksolaris .PHONY

mkvarsyesno: .PHONY
.for i in ${_MKVARS.yes}
	@echo $i="${$i}"
.endfor
.for i in ${_MKVARS.no}
	@echo $i="${$i}"
.endfor

mkextravars: .PHONY
.for i in ${MKEXTRAVARS}
	@echo $i="${$i}"
.endfor

mksolaris: .PHONY
.if (${MKDTRACE} != "no" || ${MKZFS} != "no")
	@echo MKSOLARIS="yes"
.else
	@echo MKSOLARIS="no"
.endif
