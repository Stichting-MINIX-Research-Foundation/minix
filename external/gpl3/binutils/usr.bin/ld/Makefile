#	$NetBSD: Makefile,v 1.33 2020/04/04 01:34:30 christos Exp $

CPPFLAGS+=	-I${.CURDIR}/arch/${BINUTILS_MACHINE_ARCH} \
		-I${DIST}/ld \
		-DDEFAULT_EMULATION=\"${G_EMUL}\" \
		-DSCRIPTDIR=\"${SCRIPTSDIR}\" \
		-DTARGET=\"${G_target_alias}\" \
		-DBINDIR=\"/usr/bin\" \
		-DTOOLBINDIR=\"/usr/bin\" \
		-DENABLE_PLUGINS \
		-DELF_LIST_OPTIONS=TRUE \
		-DELF_SHLIB_LIST_OPTIONS=TRUE \
		-DELF_PLT_UNWIND_LIST_OPTIONS=TRUE

.if defined(__MINIX)
CPPFLAGS+=	\
	-DTARGET_SYSTEM_ROOT=\"/usr/${G_target_alias}/sys-root\" \
	-DTARGET_SYSTEM_ROOT_RELOCATABLE
.endif # !defined(__MINIX)

.include "${.CURDIR}/../common/Makefile.inc"
.include "${.CURDIR}/arch/${BINUTILS_MACHINE_ARCH}/defs.mk"

SCRIPTSDIR=	/usr/libdata

PROG=		ld
SRCS=		${G_OFILES:.o=.c}

COPTS.ldgram.c = -Wno-stack-protector

PROGDPLIBS+=	gnuctf 	${TOP}/${BFDSUBDIR}/libgnuctf
PROGDPLIBS+=	bfd	${TOP}/${BFDSUBDIR}/libbfd
PROGDPLIBS+=	iberty 	${TOP}/${BFDSUBDIR}/libiberty

TEXINFO=	${G_TEXINFOS}
COMMONOBJDIR!=	cd ${TOP}/usr.bin/common && ${PRINTOBJDIR}
INFOFLAGS=	-I${COMMONOBJDIR} -I${DIST}/ld -I${DIST}/bfd/doc -I${DIST}/libiberty

FILESDIR=	${SCRIPTSDIR}/ldscripts

.PATH: ${COMMONOBJDIR} ${DIST}/ld ${DIST}/ld/emulparams \
	${DIST}/ld/emultempl ${DIST}/ld/scripttempl ldscripts

BUILDSYMLINKS+=	${G_STRINGIFY} stringify.sed
BUILDSYMLINKS+=	gen-doc.texi configdoc.texi

${PROG}.info: configdoc.texi bfdver.texi

.for f in ${G_EMULATION_OFILES:S/^e//:R}
.if exists(.depend.${f}) && !(make(clean) || make(cleandir))
.include ".depend.${f}"
.endif

.depend.${f}: ${f}.sh
	${_MKTARGET_CREATE}
	(srcdir=${DIST}/ld EMULATION_NAME=$f . ${>:M*.sh} && \
		if [ x"$$TEMPLATE_NAME" != x ]; then \
			temp_name=$$TEMPLATE_NAME.em; \
		else \
			temp_name=; \
		fi; \
		if [ x"$$SCRIPT_NAME" != x ]; then \
			script=$$SCRIPT_NAME.sc; \
		else \
			script=; \
		fi; \
		echo "e${f}.c: $$temp_name $$script" \
	) >$@

DPSRCS+=	.depend.${f} e${f}.c
CLEANFILES+=	.depend.${f} e${f}.c

e${f}.c: ${DIST}/ld/genscripts.sh ${.CURDIR}/Makefile stringify.sed
	${_MKTARGET_CREATE}
	unset MACHINE || true; \
	NATIVE=yes ${HOST_SH} ${DIST}/ld/genscripts.sh ${DIST}/ld \
		${LIBDIR} "/usr" "/usr/bin" \
		${G_target_alias} ${G_target_alias} ${G_target_alias} \
		"" "" ${G_EMUL} ${LIBDIR} yes ${G_enable_initfini_array} \
		${f} "${G_target_alias}"

. if ${BINUTILS_MACHINE_ARCH} == "x86_64" || \
    ${BINUTILS_MACHINE_ARCH} == "sparc64" || \
    ${BINUTILS_MACHINE_ARCH} == "riscv64" || \
    !empty(BINUTILS_MACHINE_ARCH:Maarch64*) || \
    !empty(BINUTILS_MACHINE_ARCH:Mearm*) || \
    !empty(BINUTILS_MACHINE_ARCH:Marm*) || \
    !empty(BINUTILS_MACHINE_ARCH:Mmips64*)
.  for e in x xbn xc xd xdc xdw xn xr xs xsc xsw xu xw
.   if exists(ldscripts/${f}.${e})
FILES+= ${f}.${e}
.   endif
.  endfor
. endif
.endfor

.include <bsd.prog.mk>
.include <bsd.info.mk>
.include <bsd.subdir.mk>

# Make sure we use the pre-generated C files
.l.c .y.c .y.h:
	@true

cleandir: __cleanldscripts 
__cleanldscripts:
	-rm -rf ldscripts tmpdir

.if !defined(__MINIX)
LDADD+=		-lintl -lz
DPADD+=		${LIBINTL} ${LIBZ}
.else
LDADD+=		-lz
DPADD+=		${LIBZ}
.endif # !defined(__MINIX)
