#	$NetBSD: Makefile,v 1.170 2013/09/02 14:34:57 joerg Exp $

.include <bsd.own.mk>
.include <bsd.endian.mk>

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
.if defined(HAVE_GCC)
TOOLCHAIN_BITS= gmake .WAIT
.endif

.if defined(HAVE_GCC)
.if ${HAVE_GCC} >= 45
TOOLCHAIN_BITS+= gmp .WAIT
TOOLCHAIN_BITS+= mpfr .WAIT
TOOLCHAIN_BITS+= mpc .WAIT
.endif
.endif
.endif

.if ${TOOLCHAIN_MISSING} == "no"
TOOLCHAIN_BITS+= binutils .WAIT
.endif

.if defined(HAVE_GCC)
.if ${TOOLCHAIN_MISSING} == "no"
TOOLCHAIN_BITS+= gcc
.  if ${MKCROSSGDB:Uno} != "no" # LSC: Doesn't work || make(obj)
TOOLCHAIN_BITS+= gdb
.  endif
TOOLCHAIN_BITS+= .WAIT
.endif
.endif

.if defined(HAVE_PCC)
.if ${TOOLCHAIN_MISSING} == "no"
TOOLCHAIN_BITS+= pcc
.endif
.endif

.if !defined(__MINIX)
.if ${TOOLCHAIN_MISSING} == "no"
# XXX Eventually, we want to be able to build dbsym and mdsetimage
# XXX if EXTERNAL_TOOLCHAIN is set.
TOOLCHAIN_BITS+= dbsym mdsetimage
.endif
.endif # !defined(__MINIX)

DTRACE_BITS=
.if ${MKDTRACE} != "no"
DTRACE_BITS+= .WAIT libelf
DTRACE_BITS+= .WAIT libdwarf
DTRACE_BITS+= .WAIT libctf
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
	.WAIT mktemp .WAIT sed .WAIT
.if ${TOOLS_BUILDRUMP} == "no"
SUBDIR+= genassym \
		${LINT_BITS} \
		makewhatis mtree nbperf .WAIT
.endif

SUBDIR+= join lorder m4 mkdep tsort .WAIT yacc .WAIT awk .WAIT lex

.if ${TOOLS_BUILDRUMP} == "no"
SUBDIR += .WAIT texinfo \
	.WAIT tic \
	.WAIT pax \
	.WAIT ${TOOLCHAIN_BITS} \
	${DTRACE_BITS} \
		 cat cksum \
		file \
		.WAIT \
		.WAIT \
		.WAIT \
		.WAIT \
		.WAIT \
		pwd_mkdb stat strfile zic
.endif
.if defined(__MINIX)
SUBDIR += \
	mkfs.mfs \
	partition \
	toproto \
	writeisofs
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
.if ${MKLLD} != "no"
SUBDIR+=	llvm-lld
.endif
.if ${MKMCLINKER} != "no"
SUBDIR+=	llvm-mcld
.endif
.endif

.if ${MKMAN} != "no" || ${MKDOC} != "no" || ${MKHTML} != "no"
.  if ${MKGROFF} != "no"
SUBDIR+=	groff
.  endif
SUBDIR+=	mandoc
.endif

.if ${TOOLS_BUILDRUMP} == "no"

.if ${MKMAINTAINERTOOLS:Uno} != "no"
SUBDIR+=	autoconf .WAIT gettext
.endif

.if ${USE_PIGZGZIP} != "no"
SUBDIR+=	pigz
.endif

.if ${MACHINE} == "hp700"
SUBDIR+=	hp700-mkboot
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
