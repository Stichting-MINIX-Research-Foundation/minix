#	$NetBSD: Makefile,v 1.11 2011/09/25 04:32:47 christos Exp $

.include <bsd.own.mk>

TOP=		${NETBSDSRCDIR}/external/gpl3/binutils

.include "${.CURDIR}/../common/Makefile.inc"
.include "${.CURDIR}/arch/${MACHINE_ARCH}/defs.mk"

SCRIPTSDIR=	/usr/libdata

PROG=		ld
SRCS=		${G_OFILES:.o=.c}
CPPFLAGS+=	-I${.CURDIR}/arch/${MACHINE_ARCH} \
		-I${DIST}/ld \
		-DDEFAULT_EMULATION=\"${G_EMUL}\" \
		-DSCRIPTDIR=\"${SCRIPTSDIR}\" \
		-DTARGET=\"${G_target_alias}\" \
		-DBINDIR=\"/usr/bin\" \
		-DTOOLBINDIR=\"/usr/bin\" \
		-DENABLE_PLUGINS

.if !defined(__MINIX)
LDADD=		-lintl
DPADD=		${LIBINTL}
.else
CPPFLAGS+=	\
	-DTARGET_SYSTEM_ROOT=\"/usr/${G_target_alias}/sys-root\" \
	-DTARGET_SYSTEM_ROOT_RELOCATABLE
.endif # !defined(__MINIX

COPTS.ldgram.c = -Wno-stack-protector

BFDOBJ!=	cd ${TOP}/${BFDSUBDIR}/libbfd && ${PRINTOBJDIR}
LDADD+=		-L${BFDOBJ} -lbfd
.if ${MKPICLIB} != "no"
DPADD+=		${BFDOBJ}/libbfd_pic.a
.else
DPADD+=		${BFDOBJ}/libbfd.a
.endif

IBERTYOBJ!=	cd ${TOP}/${BFDSUBDIR}/libiberty && ${PRINTOBJDIR}
LDADD+=		-L${IBERTYOBJ} -liberty
DPADD+=		${IBERTYOBJ}/libiberty.a

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

# XXX super hack
. if (${MACHINE_ARCH} == "x86_64" && \
      ("${f}" == "elf_i386" || "${f}" == "i386nbsd"))
EMUL_LIB_PATH.${f}=/usr/lib/i386
. elif (${MACHINE_ARCH} == "sparc64" && \
      ("${f}" == "elf32_sparc" || "${f}" == "sparcnbsd"))
EMUL_LIB_PATH.${f}=/usr/lib/sparc
. elif ${MACHINE_ARCH} == "mips64el"
.  if "${f}" == "elf32ltsmip" || "${f}" == "elf32btsmip"
EMUL_LIB_PATH.${f}:=/usr/lib/o32
.  elif "${f}" == "elf64ltsmip" || "${f}" == "elf64btsmip"
EMUL_LIB_PATH.${f}:=/usr/lib/64
.  else
EMUL_LIB_PATH.${f}=/usr/lib
.  endif
. else
EMUL_LIB_PATH.${f}=/usr/lib
. endif

# Binutils 2.22: the arguments have slightly changed.
#ENSCRIPTS = LIB_PATH='${LIB_PATH}' $(SHELL) $(srcdir)/genscripts.sh "${srcdir}" "${libdir}" "${prefix}" "${exec_prefix}" @host@ @target@ @target_alias@ "@EMULATION_LIBPATH@" "@NATIVE_LIB_DIRS@" @use_sysroot@ @enable_initfini_array@ @EMULATION_NAME@
e${f}.c: ${DIST}/ld/genscripts.sh ${.CURDIR}/Makefile stringify.sed
	${_MKTARGET_CREATE}
	unset MACHINE || true; \
	LIB_PATH=${EMUL_LIB_PATH.${f}} NATIVE=yes \
		${HOST_SH} ${DIST}/ld/genscripts.sh ${DIST}/ld ${LIBDIR} "/usr" "/usr/bin" \
		${G_target_alias} ${G_target_alias} ${G_target_alias} \
		${G_EMUL} ${LIBDIR} yes no ${f}

.if ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "mips64eb"
FILES+=		${f}.x ${f}.xbn ${f}.xn ${f}.xr ${f}.xu

# XXX hack to find out if .xs/.xc exists - slow!
HAS_XS!=	grep '^GENERATE_SHLIB_SCRIPT' ${DIST}/ld/emulparams/${f}.sh || echo
# XXX
#HAS_XC!=	grep '^GENERATE_COMBRELOC_SCRIPT' ${DIST}/ld/emulparams/${f}.sh || echo
.if !empty(HAS_XS)
FILES+= ${f}.xc ${f}.xd ${f}.xdc ${f}.xdw ${f}.xs ${f}.xsc ${f}.xsw ${f}.xw
# only do this if we don't have GENERATE_SHLIB_SCRIPT
.elif ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "mips64eb"
FILES+= ${f}.xc ${f}.xs ${f}.xsc ${f}.xsw ${f}.xw
.endif
.endif	# x64_64 || sparc64 || mips64

.endfor

.include <bsd.prog.mk>
.include <bsd.info.mk>
.include <bsd.subdir.mk>

# Make sure we use the pre-generated C files
.l.c .y.c .y.h:
	@true

cleanprog: __cleanldscripts 
__cleanldscripts:
	-rm -rf ldscripts tmpdir
