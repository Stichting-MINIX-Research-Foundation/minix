#	$NetBSD: Makefile,v 1.182 2015/07/23 08:03:26 mrg Exp $

.include <bsd.own.mk>
.include <bsd.endian.mk>

.if defined(__MINIX)
# Some tools do not compile warning free with newer toolchains.
NOGCCERROR=	yes
NOCLANGERROR=	yes
.endif # defined(__MINIX)
# Make sure that the ordered build/install processing applies when using
# plain make.
.MAIN: build_install

# TOOLDIR must be valid, unless MKTOOLS=no
.if ${MKTOOLS:Uyes} != "no"
.if "${TOOLDIR}" == ""
.error "TOOLDIR is undefined or empty"
.elif "${TOOLDIR:tW:M/*}" == ""
.error "TOOLDIR is not an absolute path: ${TOOLDIR}"
#.elif !exists(TOOLDIR) # XXX .exists fails for directories
#.error "TOOLDIR does not exist: ${TOOLDIR}"
.endif
.endif # MKTOOLS != no

# TOOLS_BUILDRUMP == yes builds only the subset of the tools required
# for building rump kernels and the hypervisor.  It is typically used
# when building rump kernels targeted for non-NetBSD systems (via
# buildrump.sh), and should not be set for a regular "make build".
TOOLS_BUILDRUMP?=no

.if ${TOOLCHAIN_MISSING} == "no"
. if defined(HAVE_GCC) && ${HAVE_GCC} > 0
TOOLCHAIN_BITS= gmake .WAIT
TOOLCHAIN_BITS+= gmp .WAIT
TOOLCHAIN_BITS+= mpfr .WAIT
TOOLCHAIN_BITS+= mpc .WAIT
. endif
.endif

.if ${TOOLCHAIN_MISSING} == "no"
TOOLCHAIN_BITS+= binutils .WAIT
.endif

.if defined(HAVE_GCC) && ${HAVE_GCC} > 0
. if ${TOOLCHAIN_MISSING} == "no"
TOOLCHAIN_BITS+= gcc
.if !defined(__MINIX)
# The following test is broken as it still tries to create the directory
.  if ${MKCROSSGDB:Uno} != "no" || make(obj)
TOOLCHAIN_BITS+= gdb
.  endif
.endif # !defined(__MINIX)
TOOLCHAIN_BITS+= .WAIT
. endif
.endif

.if defined(HAVE_PCC)
. if ${TOOLCHAIN_MISSING} == "no"
TOOLCHAIN_BITS+= pcc
. endif
.endif

.if !defined(__MINIX)
.if ${TOOLCHAIN_MISSING} == "no"
# XXX Eventually, we want to be able to build dbsym and mdsetimage
# XXX if EXTERNAL_TOOLCHAIN is set.
TOOLCHAIN_BITS+= dbsym mdsetimage
.endif
.endif # !defined(__MINIX)

DTRACE_BITS=
.if ${MKDTRACE} != "no" || ${MKCTF} != "no"
DTRACE_BITS+= .WAIT libelf
DTRACE_BITS+= .WAIT libdwarf
DTRACE_BITS+= .WAIT libctf
.endif
.if ${MKCTF} != "no"
DTRACE_BITS+= .WAIT ctfconvert ctfmerge
.endif

LINT_BITS=
.if ${MKLINT} != "no"
LINT_BITS= lint lint2
.endif

# All of host-mkdep, compat, and binstall are needed before anything
# else.  Within this group, they must be built in a specific order, and
# all of them must be built before any of them is installed.  They may
# be installed in any order.  This can't be expressed using the .WAIT
# notation inside the SUBDIR list.
#
# XXX .ORDER does not work when multiple targets are passed on the
# make command line without "-j", so use dependencies in addition to .ORDER.
#
.ORDER: dependall-host-mkdep dependall-compat dependall-binstall
.if make(dependall-host-mkdep) && make(dependall-compat)
dependall-compat: dependall-host-mkdep
.endif
.if make(dependall-compat) && make(dependall-binstall)
dependall-binstall: dependall-compat
.endif

# Dependencies in SUBDIR below ordered to maximize parallel ability.
# See above for special treatment for host-mkdep, compat, and binstall.
#
SUBDIR=	host-mkdep compat binstall \
	.WAIT mktemp .WAIT sed .WAIT genassym
.if ${TOOLS_BUILDRUMP} == "no"
SUBDIR+=  gencat \
		${LINT_BITS} \
		makewhatis mtree nbperf .WAIT uudecode
.endif

SUBDIR+= cat join lorder m4 mkdep tsort .WAIT yacc .WAIT awk .WAIT lex

.if ${TOOLS_BUILDRUMP} == "no"
SUBDIR += .WAIT texinfo \
	.WAIT tic \
	.WAIT pax \
	.WAIT ${TOOLCHAIN_BITS} \
	${DTRACE_BITS} \
		 cksum \
		file \
		makefs .WAIT mkcsmapper mkesdb mklocale \
		.WAIT \
		.WAIT \
		.WAIT \
		.WAIT installboot \
		pwd_mkdb strfile zic
.endif
SUBDIR+= stat .WAIT

.if defined(__MINIX)
SUBDIR += \
	mkfs.mfs \
	partition \
	toproto
.else
SUBDIR+= .WAIT config
.endif # defined(__MINIX)

.if ${MKLLVM} != "no"
SUBDIR+= \
	llvm .WAIT \
	llvm-lib/libLLVMSupport llvm-lib/libLLVMTableGen .WAIT \
	llvm-tblgen llvm-clang-tblgen .WAIT \
	llvm-include .WAIT \
	llvm-lib .WAIT \
	llvm-clang
. if ${MKLLD} != "no"
SUBDIR+=	llvm-lld
. endif
. if ${MKMCLINKER} != "no"
SUBDIR+=	llvm-mcld
. endif
.endif

.if ${MKMAN} != "no" || ${MKDOC} != "no" || ${MKHTML} != "no"
. if ${MKGROFF} != "no"
SUBDIR+=	groff
. endif
SUBDIR+=	mandoc
.endif

.if ${TOOLS_BUILDRUMP} == "no"

.if ${MKMAINTAINERTOOLS:Uno} != "no"
SUBDIR+=	autoconf .WAIT gettext
.endif

.if ${USE_PIGZGZIP} != "no"
SUBDIR+=	pigz
.endif

.if ${MACHINE} == "hppa"
SUBDIR+=	hppa-mkboot
.endif

.if ${MACHINE} == "ibmnws"
SUBDIR+=	ibmnws-ncdcs
.endif

.if ${MACHINE} == "macppc"
SUBDIR+=	macppc-fixcoff
.endif

.if (${MACHINE} == "prep" || ${MACHINE} == "rs6000" || ${MACHINE} == "bebox")
SUBDIR+=	powerpc-mkbootimage
.endif

.if ${MACHINE_ARCH} == "m68k"
SUBDIR+=	m68k-elf2aout
.endif

.if (${MACHINE_ARCH} == "mipsel" || ${MACHINE_ARCH} == "mipseb" || \
     ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "mips64eb")
SUBDIR+=	mips-elf2ecoff
.endif

.if (${MACHINE} == "sgimips")
SUBDIR+=	sgivol
.endif

.if ${MACHINE} == "acorn32"
SUBDIR+=	sparkcrc
.endif

.if (${MACHINE_ARCH} == "sparc" || ${MACHINE_ARCH} == "sparc64")
SUBDIR+=	fgen
.endif

.if ${MACHINE} == "amiga"
SUBDIR+=	amiga-elf2bb
SUBDIR+=	amiga-txlt
.endif

.if ${MACHINE} == "hp300"
SUBDIR+=	hp300-mkboot
.endif

.if !defined(__MINIX)
.if ${MACHINE} == "evbarm" \
    && ${MACHINE_CPU} == "arm" \
    && ${TARGET_ENDIANNESS} == "1234"
SUBDIR+=	elftosb
.endif

.if ${MACHINE} == "evbarm" || ${MACHINE} == "evbmips" || \
    ${MACHINE} == "evbppc" || ${MACHINE} == "sandpoint"
SUBDIR+=	mkubootimage
.endif
.endif # !defined(__MINIX)

.if ${MKX11} != "no"
SUBDIR+=	makestrs
SUBDIR+=	makekeys
.endif

.endif # TOOLCHAIN_BUILDRUMP

check_MKTOOLS: .PHONY .NOTMAIN
.if ${MKTOOLS:Uyes} == "no"
	@echo '*** WARNING: "MKTOOLS" is set to "no"; this will prevent building and'
	@echo '*** updating your host toolchain.  This should be used only as a'
	@echo '*** temporary workaround for toolchain problems, as it will result'
	@echo '*** in version skew and build errors over time!'
.endif

.if ${MKTOOLS:Uyes} == "no" || ${USETOOLS} != "yes"	# {
SUBDIR= # empty
realall realdepend install: check_MKTOOLS
.endif							# }

.include <bsd.subdir.mk>
.include <bsd.buildinstall.mk>
.include <bsd.obj.mk>

.if !defined(PREVIOUSTOOLDIR)
.  if exists(PREVIOUSTOOLDIR)
PREVIOUSTOOLDIR!=	cat PREVIOUSTOOLDIR
.  else
PREVIOUSTOOLDIR=
.  endif
.endif

CLEANFILES+=	PREVIOUSTOOLDIR

realall realdepend: .MAKE
.if !empty(PREVIOUSTOOLDIR) && "${PREVIOUSTOOLDIR}" != "${TOOLDIR}"
	@echo "*** WARNING: TOOLDIR has moved?"
	@echo "*** PREVIOUSTOOLDIR '${PREVIOUSTOOLDIR}'"
	@echo "***     !=  TOOLDIR '${TOOLDIR}'"
	@echo "*** Cleaning mis-matched tools"
	rm -f PREVIOUSTOOLDIR
	(cd ${.CURDIR} && ${MAKE} PREVIOUSTOOLDIR=${TOOLDIR} cleandir)
.endif
	echo ${TOOLDIR} >PREVIOUSTOOLDIR

cleandir:
	rm -f ${CLEANFILES}
