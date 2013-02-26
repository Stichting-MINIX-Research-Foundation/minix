#	$NetBSD: bsd.own.mk,v 1.706 2012/08/16 05:30:55 matt Exp $

# This needs to be before bsd.init.mk
.if defined(BSD_MK_COMPAT_FILE)
.include <${BSD_MK_COMPAT_FILE}>
.endif

.if !defined(_BSD_OWN_MK_)
_BSD_OWN_MK_=1

.if defined(__MINIX)

# Some Minix deviations from NetBSD
LDSTATIC?=	-static
MKDYNAMICROOT?=	no
NO_LIBGOMP?=	yes

BINMODE?=	755
NONBINMODE?=	644
MANDIR?=	/usr/man
BINGRP?=	operator
MANGRP?=	operator
INFOGRP?=	operator
DOCGRP?=	operator

MKBINUTILS?=	no
MKGDB:=		no
MKGCC?=		no
MKGCCCMDS?=	no
MKSLJIT?=	no

# LSC MINIX SMP Support?
.ifdef CONFIG_SMP
SMP_FLAGS += -DCONFIG_SMP
.ifdef CONFIG_MAX_CPUS
SMP_FLAGS += -DCONFIG_MAX_CPUS=${CONFIG_MAX_CPUS}
.endif
.endif

CPPFLAGS+= ${SMP_FLAGS}

__uname_s!= uname -s
.if ${__uname_s:Uunknown} == "Minix" 
USETOOLS?=	never
.  if ${USETOOLS:Uno} != "yes"
.    if ${_HAVE_LLVM:U} == ""
       _HAVE_LLVM!= (exec 2>&1; clang --version || echo "")
       _HAVE_LLVM:= ${_HAVE_LLVM:M[0-9]\.[0-9]}
.      if ${_HAVE_LLVM} != ""
         HAVE_LLVM?= ${_HAVE_LLVM}
.      endif
.    endif # ${_HAVE_LLVM:U} == ""

.    if ${_HAVE_GOLD:U} == ""
       _HAVE_GOLD!= (exec 2>&1; ld --version || echo "")
       _GOLD_MATCH:=${_HAVE_GOLD:Mgold}
       _HAVE_GOLD:= ${_HAVE_GOLD:M[0-9]\.[0-9][0-9]}
.      if ${_GOLD_MATCH} != "" && ${_HAVE_GOLD} != ""
          HAVE_GOLD?= ${_HAVE_GOLD}
          CFLAGS+= -DHAVE_GOLD=${_HAVE_GOLD}
          AFLAGS+= -DHAVE_GOLD=${_HAVE_GOLD}
.      else
          MKBITCODE:=no
.      endif
.    endif # ${_HAVE_GOLD:U} == ""
.  endif # ${USETOOLS:Uno} != "yes"

.  if !defined(HOSTPROG) && !defined(HOSTLIB)
# LSC FIXME: Override MACHINE as the native minix host make command will set 
#            it to i686.
.    if ${MACHINE_ARCH} == "i386"
       MACHINE:= i386
.    endif
.  endif # !defined(HOSTPROG) && !defined(HOSTLIB)
.endif # __uname_s == "Minix"

#
# linker script files that have to be specified explicitely for the gold linker
#

LDS_PATH=		${NETBSDSRCDIR}/external/gpl3/binutils/ldscripts
LDS_STATIC_BIN=		${LDS_PATH}/elf_${MACHINE_ARCH}_minix.xc
LDS_DYNAMIC_BIN=	${LDS_PATH}/elf_${MACHINE_ARCH}_minix.xc
LDS_RELOC=		${LDS_PATH}/elf_${MACHINE_ARCH}_minix.xr
LDS_SHARED_LIB=		${LDS_PATH}/elf_${MACHINE_ARCH}_minix.xsc
LDS_N=			${LDS_PATH}/elf_${MACHINE_ARCH}_minix.xbn

# LSC In the current state there is too much to be done
# Some package have been identified by directly adding NOGCCERROR
# To their Makefiles
NOGCCERROR?=	yes
NOCLANGERROR?=	yes

AFLAGS+=	-D__ASSEMBLY__
CFLAGS+=	-fno-builtin

# Compile Kyua only if we have MKGCCCMDS, as we need libstdc++ from GCC
.if ${MKGCCCMDS:Uno} == "no"
MKATF:=		no
.else
# For C++ programs, only if we have the GCC sources
CXXFLAGS+=	-I${DESTDIR}/usr/include/g++
MKKYUA?=	yes
.endif #  ${MKGCCCMDS:Uno} == "no"

#LSC FIXME: Needed by clang for now
.if ${MACHINE_ARCH} == "i386"
CPUFLAGS+=	-march=i586
.endif
.endif # defined(__MINIX)

MAKECONF?=	/etc/mk.conf
.-include "${MAKECONF}"

#
# CPU model, derived from MACHINE_ARCH
#
MACHINE_CPU=	${MACHINE_ARCH:C/mipse[bl]/mips/:C/mips64e[bl]/mips/:C/sh3e[bl]/sh3/:S/m68000/m68k/:S/armeb/arm/:S/earm/arm/:S/earmeb/arm/:S/powerpc64/powerpc/}

#
# Subdirectory used below ${RELEASEDIR} when building a release
#
RELEASEMACHINEDIR?=	${MACHINE}

#
# Subdirectory or path component used for the following paths:
#   distrib/${RELEASEMACHINE}
#   distrib/notes/${RELEASEMACHINE}
#   etc/etc.${RELEASEMACHINE}
# Used when building a release.
#
RELEASEMACHINE?=	${MACHINE}

#
# NEED_OWN_INSTALL_TARGET is set to "no" by pkgsrc/mk/bsd.pkg.mk to
# ensure that things defined by <bsd.own.mk> (default targets,
# INSTALL_FILE, etc.) are not conflicting with bsd.pkg.mk.
#
NEED_OWN_INSTALL_TARGET?=	yes

#
# This lists the platforms which do not have working in-tree toolchains.  For
# the in-tree gcc toolchain, this list is empty.
#
# If some future port is not supported by the in-tree toolchain, this should
# be set to "yes" for that port only.
#
TOOLCHAIN_MISSING?=	no

#
# Platforms still using GCC 4.1
#
.if ${MKGCC:Uyes} != "no"
.if ${MACHINE_CPU}  == "vax"
HAVE_GCC?=    4
.else
# Otherwise, default to GCC4.5
HAVE_GCC?=    45
.endif
.endif

.if \
    ${MACHINE_CPU} == "arm" || \
    ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "powerpc" || \
    ${MACHINE_CPU} == "sh3" || \
    ${MACHINE_ARCH} == "x86_64"
USE_COMPILERCRTSTUFF?=	no
.endif
USE_COMPILERCRTSTUFF?=	yes


#
# Platforms still using GDB 6
#
.if ${MACHINE_CPU}  == "mips"
HAVE_GDB?= 6
.else
# Otherwise, default to GDB7
HAVE_GDB?=	7
.endif

.if (${MACHINE_ARCH} == "alpha") || \
    (${MACHINE_ARCH} == "hppa") || \
    (${MACHINE_ARCH} == "ia64") || \
    (${MACHINE_ARCH} == "mipsel") || (${MACHINE_ARCH} == "mipseb") || \
    (${MACHINE_ARCH} == "mips64el") || (${MACHINE_ARCH} == "mips64eb")
HAVE_SSP?=	no
.else
HAVE_SSP?=	yes
.if ${USE_FORT:Uno} != "no"
USE_SSP?=	yes
.endif
.endif


.if empty(.MAKEFLAGS:M-V*)
.if defined(MAKEOBJDIRPREFIX) || defined(MAKEOBJDIR)
PRINTOBJDIR=	${MAKE} -r -V .OBJDIR -f /dev/null xxx
.else
PRINTOBJDIR=	${MAKE} -V .OBJDIR
.endif
.else
PRINTOBJDIR=	echo # prevent infinite recursion
.endif

#
# Determine if running in the NetBSD source tree by checking for the
# existence of build.sh and tools/ in the current or a parent directory,
# and setting _SRC_TOP_ to the result.
#
.if !defined(_SRC_TOP_)			# {
_SRC_TOP_!= cd "${.CURDIR}"; while :; do \
		here=`pwd`; \
		[ -f build.sh  ] && [ -d tools ] && { echo $$here; break; }; \
		case $$here in /) echo ""; break;; esac; \
		cd ..; done

.MAKEOVERRIDES+=	_SRC_TOP_

.endif					# }

#
# If _SRC_TOP_ != "", we're within the NetBSD source tree, so set
# defaults for NETBSDSRCDIR and _SRC_TOP_OBJ_.
#
.if (${_SRC_TOP_} != "")		# {

NETBSDSRCDIR?=	${_SRC_TOP_}

.if !defined(_SRC_TOP_OBJ_)
_SRC_TOP_OBJ_!=		cd "${_SRC_TOP_}" && ${PRINTOBJDIR}
.MAKEOVERRIDES+=	_SRC_TOP_OBJ_
.endif

.endif	# _SRC_TOP_ != ""		# }


.if (${_SRC_TOP_} != "") && \
    (${TOOLCHAIN_MISSING} == "no" || defined(EXTERNAL_TOOLCHAIN))
USETOOLS?=	yes
.endif
USETOOLS?=	no


.if ${MACHINE_ARCH} == "mips" || ${MACHINE_ARCH} == "mips64" || \
    ${MACHINE_ARCH} == "sh3"
.BEGIN:
	@echo "Must set MACHINE_ARCH to one of ${MACHINE_ARCH}eb or ${MACHINE_ARCH}el"
	@false
.elif defined(REQUIRETOOLS) && \
      (${TOOLCHAIN_MISSING} == "no" || defined(EXTERNAL_TOOLCHAIN)) && \
      ${USETOOLS} == "no"
.BEGIN:
	@echo "USETOOLS=no, but this component requires a version-specific host toolchain"
	@false
.endif

#
# Host platform information; may be overridden
#
.if !defined(HOST_OSTYPE)
_HOST_OSNAME!=	uname -s
_HOST_OSREL!=	uname -r
# For _HOST_ARCH, if uname -p fails, or prints "unknown", or prints
# something that does not look like an identifier, then use uname -m.
_HOST_ARCH!=	uname -p 2>/dev/null
_HOST_ARCH:=	${HOST_ARCH:tW:C/.*[^-_A-Za-z0-9].*//:S/unknown//}
.if empty(_HOST_ARCH)
_HOST_ARCH!=	uname -m
.endif
HOST_OSTYPE:=	${_HOST_OSNAME}-${_HOST_OSREL:C/\([^\)]*\)//g:[*]:C/ /_/g}-${_HOST_ARCH:C/\([^\)]*\)//g:[*]:C/ /_/g}
.MAKEOVERRIDES+= HOST_OSTYPE
.endif # !defined(HOST_OSTYPE)

.if ${USETOOLS} == "yes"						# {

#
# Provide a default for TOOLDIR.
#
.if !defined(TOOLDIR)
TOOLDIR:=	${_SRC_TOP_OBJ_}/tooldir.${HOST_OSTYPE}
.MAKEOVERRIDES+= TOOLDIR
.endif

#
# This is the prefix used for the NetBSD-sourced tools.
#
_TOOL_PREFIX?=	nb

#
# If an external toolchain base is specified, use it.
#
.if defined(EXTERNAL_TOOLCHAIN)						# {
AR=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-ar
AS=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-as
LD=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-ld
NM=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-nm
OBJCOPY=	${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-objcopy
OBJDUMP=	${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-objdump
RANLIB=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-ranlib
SIZE=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-size
STRIP=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-strip

TOOL_CC.gcc=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-gcc
TOOL_CPP.gcc=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-cpp
TOOL_CXX.gcc=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-c++
TOOL_FC.gcc=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-g77
TOOL_OBJC.gcc=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-gcc
.else									# } {
# Define default locations for common tools.
.if ${USETOOLS_BINUTILS:Uyes} == "yes"					#  {
AR=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ar
AS=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-as
LD=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ld
NM=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-nm
OBJCOPY=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-objcopy
OBJDUMP=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-objdump
RANLIB=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-ranlib
SIZE=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-size
STRIP=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-strip

# GCC supports C, C++, Fortran and Objective C
TOOL_CC.gcc=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-gcc
TOOL_CPP.gcc=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-cpp
TOOL_CXX.gcc=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-c++
TOOL_FC.gcc=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-g77
TOOL_OBJC.gcc=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-gcc
.endif									#  }

# Clang supports C, C++ and Objective C
TOOL_CC.clang=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-clang
TOOL_CPP.clang=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-clang-cpp
TOOL_CXX.clang=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-clang++
TOOL_OBJC.clang=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-clang

# PCC supports C and Fortran
TOOL_CC.pcc=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-pcc
TOOL_CPP.pcc=		${TOOLDIR}/libexec/${MACHINE_GNU_PLATFORM}-cpp

#
# Make sure DESTDIR is set, so that builds with these tools always
# get appropriate -nostdinc, -nostdlib, etc. handling.  The default is
# <empty string>, meaning start from /, the root directory.
#
DESTDIR?=

.if !defined(HOSTPROG) && !defined(HOSTLIB)
.  if ${DESTDIR} != ""
CPPFLAGS+=	--sysroot=${DESTDIR}
LDFLAGS+=	--sysroot=${DESTDIR}
.  else
CPPFLAGS+=	--sysroot=/
LDFLAGS+=	--sysroot=/
.  endif
.endif
.endif	# EXTERNAL_TOOLCHAIN						# }

HOST_MKDEP=	${TOOLDIR}/bin/${_TOOL_PREFIX}host-mkdep

DBSYM=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-dbsym
ELF2AOUT=	${TOOLDIR}/bin/${_TOOL_PREFIX}m68k-elf2aout
ELF2ECOFF=	${TOOLDIR}/bin/${_TOOL_PREFIX}mips-elf2ecoff
INSTALL=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-install
LEX=		${TOOLDIR}/bin/${_TOOL_PREFIX}lex
LINT=		CC=${CC:Q} ${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-lint
LORDER=		NM=${NM:Q} MKTEMP=${TOOL_MKTEMP:Q} ${TOOLDIR}/bin/${_TOOL_PREFIX}lorder
MKDEP=		CC=${CC:Q} ${TOOLDIR}/bin/${_TOOL_PREFIX}mkdep
PAXCTL=		${TOOLDIR}/bin/${_TOOL_PREFIX}paxctl
TSORT=		${TOOLDIR}/bin/${_TOOL_PREFIX}tsort -q
YACC=		${TOOLDIR}/bin/${_TOOL_PREFIX}yacc

TOOL_AMIGAAOUT2BB=	${TOOLDIR}/bin/${_TOOL_PREFIX}amiga-aout2bb
TOOL_AMIGAELF2BB=	${TOOLDIR}/bin/${_TOOL_PREFIX}amiga-elf2bb
TOOL_AMIGATXLT=		${TOOLDIR}/bin/${_TOOL_PREFIX}amiga-txlt
TOOL_ASN1_COMPILE=	${TOOLDIR}/bin/${_TOOL_PREFIX}asn1_compile
TOOL_AWK=		${TOOLDIR}/bin/${_TOOL_PREFIX}awk
TOOL_CAP_MKDB=		${TOOLDIR}/bin/${_TOOL_PREFIX}cap_mkdb
TOOL_CAT=		${TOOLDIR}/bin/${_TOOL_PREFIX}cat
TOOL_CKSUM=		${TOOLDIR}/bin/${_TOOL_PREFIX}cksum
TOOL_CLANG_TBLGEN=		${TOOLDIR}/bin/${_TOOL_PREFIX}clang-tblgen
TOOL_COMPILE_ET=	${TOOLDIR}/bin/${_TOOL_PREFIX}compile_et
TOOL_CONFIG=		${TOOLDIR}/bin/${_TOOL_PREFIX}config
TOOL_CRUNCHGEN=		MAKE=${.MAKE:Q} ${TOOLDIR}/bin/${_TOOL_PREFIX}crunchgen
TOOL_CTAGS=		${TOOLDIR}/bin/${_TOOL_PREFIX}ctags
TOOL_CTFCONVERT=	${TOOLDIR}/bin/${_TOOL_PREFIX}ctfconvert
TOOL_CTFMERGE=		${TOOLDIR}/bin/${_TOOL_PREFIX}ctfmerge
TOOL_DB=		${TOOLDIR}/bin/${_TOOL_PREFIX}db
TOOL_DISKLABEL=		${TOOLDIR}/bin/nbdisklabel-${MAKEWRAPPERMACHINE}
TOOL_EQN=		${TOOLDIR}/bin/${_TOOL_PREFIX}eqn
TOOL_FDISK=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-fdisk
TOOL_FGEN=		${TOOLDIR}/bin/${_TOOL_PREFIX}fgen
TOOL_GENASSYM=		${TOOLDIR}/bin/${_TOOL_PREFIX}genassym
TOOL_GENCAT=		${TOOLDIR}/bin/${_TOOL_PREFIX}gencat
TOOL_GMAKE=		${TOOLDIR}/bin/${_TOOL_PREFIX}gmake
TOOL_GREP=		${TOOLDIR}/bin/${_TOOL_PREFIX}grep
TOOL_GROFF=		PATH=${TOOLDIR}/lib/groff:$${PATH} ${TOOLDIR}/bin/${_TOOL_PREFIX}groff
TOOL_HEXDUMP=		${TOOLDIR}/bin/${_TOOL_PREFIX}hexdump
TOOL_HP300MKBOOT=	${TOOLDIR}/bin/${_TOOL_PREFIX}hp300-mkboot
TOOL_HP700MKBOOT=	${TOOLDIR}/bin/${_TOOL_PREFIX}hp700-mkboot
TOOL_INDXBIB=		${TOOLDIR}/bin/${_TOOL_PREFIX}indxbib
TOOL_INSTALLBOOT=	${TOOLDIR}/bin/${_TOOL_PREFIX}installboot
TOOL_INSTALL_INFO=	${TOOLDIR}/bin/${_TOOL_PREFIX}install-info
TOOL_JOIN=		${TOOLDIR}/bin/${_TOOL_PREFIX}join
TOOL_LLVM_TBLGEN=		${TOOLDIR}/bin/${_TOOL_PREFIX}llvm-tblgen
TOOL_M4=		${TOOLDIR}/bin/${_TOOL_PREFIX}m4
TOOL_MACPPCFIXCOFF=	${TOOLDIR}/bin/${_TOOL_PREFIX}macppc-fixcoff
TOOL_MAKEFS=		${TOOLDIR}/bin/${_TOOL_PREFIX}makefs
TOOL_MAKEINFO=		${TOOLDIR}/bin/${_TOOL_PREFIX}makeinfo
TOOL_MAKEWHATIS=	${TOOLDIR}/bin/${_TOOL_PREFIX}makewhatis
TOOL_MANDOC_ASCII=	${TOOLDIR}/bin/${_TOOL_PREFIX}mandoc -Tascii
TOOL_MANDOC_HTML=	${TOOLDIR}/bin/${_TOOL_PREFIX}mandoc -Thtml
TOOL_MANDOC_LINT=	${TOOLDIR}/bin/${_TOOL_PREFIX}mandoc -Tlint
TOOL_MDSETIMAGE=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-mdsetimage
TOOL_MENUC=		MENUDEF=${TOOLDIR}/share/misc ${TOOLDIR}/bin/${_TOOL_PREFIX}menuc
TOOL_M68KELF2AOUT=	${TOOLDIR}/bin/${_TOOL_PREFIX}m68k-elf2aout
TOOL_MIPSELF2ECOFF=	${TOOLDIR}/bin/${_TOOL_PREFIX}mips-elf2ecoff
TOOL_MKCSMAPPER=	${TOOLDIR}/bin/${_TOOL_PREFIX}mkcsmapper
TOOL_MKESDB=		${TOOLDIR}/bin/${_TOOL_PREFIX}mkesdb
TOOL_MKFSMFS=		${TOOLDIR}/bin/${_TOOL_PREFIX}mkfs.mfs
TOOL_MKLOCALE=		${TOOLDIR}/bin/${_TOOL_PREFIX}mklocale
TOOL_MKMAGIC=		${TOOLDIR}/bin/${_TOOL_PREFIX}file
TOOL_MKTEMP=		${TOOLDIR}/bin/${_TOOL_PREFIX}mktemp
TOOL_MKUBOOTIMAGE=	${TOOLDIR}/bin/${_TOOL_PREFIX}mkubootimage
TOOL_MSGC=		MSGDEF=${TOOLDIR}/share/misc ${TOOLDIR}/bin/${_TOOL_PREFIX}msgc
TOOL_MTREE=		${TOOLDIR}/bin/${_TOOL_PREFIX}mtree
TOOL_NBPERF=		${TOOLDIR}/bin/${_TOOL_PREFIX}perf
TOOL_PAX=		${TOOLDIR}/bin/${_TOOL_PREFIX}pax
TOOL_PIC=		${TOOLDIR}/bin/${_TOOL_PREFIX}pic
TOOL_PIGZ=		${TOOLDIR}/bin/${_TOOL_PREFIX}pigz
TOOL_PKG_CREATE=	${TOOLDIR}/bin/${_TOOL_PREFIX}pkg_create
TOOL_POWERPCMKBOOTIMAGE=${TOOLDIR}/bin/${_TOOL_PREFIX}powerpc-mkbootimage
TOOL_PWD_MKDB=		${TOOLDIR}/bin/${_TOOL_PREFIX}pwd_mkdb
TOOL_REFER=		${TOOLDIR}/bin/${_TOOL_PREFIX}refer
TOOL_ROFF_ASCII=	PATH=${TOOLDIR}/lib/groff:$${PATH} ${TOOLDIR}/bin/${_TOOL_PREFIX}nroff
TOOL_ROFF_DVI=		${TOOL_GROFF} -Tdvi
TOOL_ROFF_HTML=		${TOOL_GROFF} -Tlatin1 -mdoc2html
TOOL_ROFF_PS=		${TOOL_GROFF} -Tps
TOOL_ROFF_RAW=		${TOOL_GROFF} -Z
TOOL_RPCGEN=		RPCGEN_CPP=${CPP:Q} ${TOOLDIR}/bin/${_TOOL_PREFIX}rpcgen
TOOL_SED=		${TOOLDIR}/bin/${_TOOL_PREFIX}sed
TOOL_SLC=		${TOOLDIR}/bin/${_TOOL_PREFIX}slc
TOOL_SOELIM=		${TOOLDIR}/bin/${_TOOL_PREFIX}soelim
TOOL_SPARKCRC=		${TOOLDIR}/bin/${_TOOL_PREFIX}sparkcrc
TOOL_STAT=		${TOOLDIR}/bin/${_TOOL_PREFIX}stat
TOOL_STRFILE=		${TOOLDIR}/bin/${_TOOL_PREFIX}strfile
TOOL_SUNLABEL=		${TOOLDIR}/bin/${_TOOL_PREFIX}sunlabel
TOOL_TBL=		${TOOLDIR}/bin/${_TOOL_PREFIX}tbl
TOOL_TIC=		${TOOLDIR}/bin/${_TOOL_PREFIX}tic
TOOL_UUDECODE=		${TOOLDIR}/bin/${_TOOL_PREFIX}uudecode
TOOL_VGRIND=		${TOOLDIR}/bin/${_TOOL_PREFIX}vgrind -f
TOOL_ZIC=		${TOOLDIR}/bin/${_TOOL_PREFIX}zic

.else	# USETOOLS != yes						# } {

# Clang supports C, C++ and Objective C
TOOL_CC.clang=		clang
TOOL_CPP.clang=		clang-cpp
TOOL_CXX.clang=		clang++
TOOL_OBJC.clang=	clang

# GCC supports C, C++, Fortran and Objective C
TOOL_CC.gcc=	gcc
TOOL_CPP.gcc=	cpp
TOOL_CXX.gcc=	c++
TOOL_FC.gcc=	g77
TOOL_OBJC.gcc=	gcc

# PCC supports C and Fortran
TOOL_CC.pcc=		pcc
TOOL_CPP.pcc=		/usr/libexec/pcpp

TOOL_AMIGAAOUT2BB=	amiga-aout2bb
TOOL_AMIGAELF2BB=	amiga-elf2bb
TOOL_AMIGATXLT=		amiga-txlt
TOOL_ASN1_COMPILE=	asn1_compile
TOOL_AWK=		awk
TOOL_CAP_MKDB=		cap_mkdb
TOOL_CAT=		cat
TOOL_CKSUM=		cksum
TOOL_CLANG_TBLGEN=	clang-tblgen
TOOL_COMPILE_ET=	compile_et
TOOL_CONFIG=		config
TOOL_CRUNCHGEN=		crunchgen
TOOL_CTAGS=		ctags
TOOL_CTFCONVERT=	ctfconvert
TOOL_CTFMERGE=		ctfmerge
TOOL_DB=		db
TOOL_DISKLABEL=		disklabel
TOOL_EQN=		eqn
TOOL_FDISK=		fdisk
TOOL_FGEN=		fgen
TOOL_GENASSYM=		genassym
TOOL_GENCAT=		gencat
TOOL_GMAKE=		gmake
TOOL_GREP=		grep
TOOL_GROFF=		groff
TOOL_HEXDUMP=		hexdump
TOOL_HP300MKBOOT=	hp300-mkboot
TOOL_HP700MKBOOT=	hp700-mkboot
TOOL_INDXBIB=		indxbib
TOOL_INSTALLBOOT=	installboot
TOOL_INSTALL_INFO=	install-info
TOOL_JOIN=		join
TOOL_LLVM_TBLGEN=	llvm-tblgen
TOOL_M4=		m4
TOOL_MACPPCFIXCOFF=	macppc-fixcoff
TOOL_MAKEFS=		makefs
TOOL_MAKEINFO=		makeinfo
TOOL_MAKEWHATIS=	/usr/libexec/makewhatis
TOOL_MANDOC_ASCII=	mandoc -Tascii
TOOL_MANDOC_HTML=	mandoc -Thtml -Oman=../html%S/%N.html -Ostyle=../style.css
TOOL_MANDOC_LINT=	mandoc -Tlint
TOOL_MDSETIMAGE=	mdsetimage
TOOL_MENUC=		menuc
TOOL_M68KELF2AOUT=	m68k-elf2aout
TOOL_MIPSELF2ECOFF=	mips-elf2ecoff
TOOL_MKCSMAPPER=	mkcsmapper
TOOL_MKESDB=		mkesdb
TOOL_MKFSMFS=		mkfs.mfs
TOOL_MKLOCALE=		mklocale
TOOL_MKMAGIC=		file
TOOL_MKTEMP=		mktemp
TOOL_MKUBOOTIMAGE=	mkubootimage
TOOL_MSGC=		msgc
TOOL_MTREE=		mtree
TOOL_NBPERF=		nbperf
TOOL_PAX=		pax
TOOL_PIC=		pic
TOOL_PIGZ=		pigz
TOOL_PKG_CREATE=	pkg_create
TOOL_POWERPCMKBOOTIMAGE=	powerpc-mkbootimage
TOOL_PWD_MKDB=		pwd_mkdb
TOOL_REFER=		refer
TOOL_ROFF_ASCII=	nroff
TOOL_ROFF_DVI=		${TOOL_GROFF} -Tdvi
TOOL_ROFF_HTML=		${TOOL_GROFF} -Tlatin1 -mdoc2html
TOOL_ROFF_PS=		${TOOL_GROFF} -Tps
TOOL_ROFF_RAW=		${TOOL_GROFF} -Z
TOOL_RPCGEN=		rpcgen
TOOL_SED=		sed
TOOL_SOELIM=		soelim
TOOL_SPARKCRC=		sparkcrc
TOOL_STAT=		stat
TOOL_STRFILE=		strfile
TOOL_SUNLABEL=		sunlabel
TOOL_TBL=		tbl
TOOL_TIC=		tic
TOOL_UUDECODE=		uudecode
TOOL_VGRIND=		vgrind -f
TOOL_ZIC=		zic

.endif	# USETOOLS != yes						# }

# Fallback to ensure that all variables are defined to something
TOOL_CC.false=		false
TOOL_CPP.false=		false
TOOL_CXX.false=		false
TOOL_FC.false=		false
TOOL_OBJC.false=	false

AVAILABLE_COMPILER?=	${HAVE_PCC:Dpcc} ${HAVE_LLVM:Dclang} ${HAVE_GCC:Dgcc} false

.for _t in CC CPP CXX FC OBJC
ACTIVE_${_t}=	${AVAILABLE_COMPILER:@.c.@ ${ !defined(UNSUPPORTED_COMPILER.${.c.}) && defined(TOOL_${_t}.${.c.}) :? ${.c.} : }@:[1]}
SUPPORTED_${_t}=${AVAILABLE_COMPILER:Nfalse:@.c.@ ${ !defined(UNSUPPORTED_COMPILER.${.c.}) && defined(TOOL_${_t}.${.c.}) :? ${.c.} : }@}
.endfor
# make bugs prevent moving this into the .for loop
CC=		${TOOL_CC.${ACTIVE_CC}}
CPP=		${TOOL_CPP.${ACTIVE_CPP}}
CXX=		${TOOL_CXX.${ACTIVE_CXX}}
FC=		${TOOL_FC.${ACTIVE_FC}}
OBJC=		${TOOL_OBJC.${ACTIVE_OBJC}}

# OBJCOPY flags to create a.out binaries for old firmware
# shared among src/distrib and ${MACHINE}/conf/Makefile.${MACHINE}.inc
.if ${MACHINE_CPU} == "arm"
OBJCOPY_ELF2AOUT_FLAGS?=	\
	-O a.out-arm-netbsd	\
	-R .ident		\
	-R .ARM.attributes	\
	-R .ARM.exidx		\
	-R .arm.atpcs		\
	-R .comment		\
	-R .debug_abbrev	\
	-R .debug_info		\
	-R .debug_line		\
	-R .debug_frame		\
	-R .debug_loc		\
	-R .debug_pubnames	\
	-R .debug_aranges	\
	-R .debug_str		\
	-R .debug_pubtypes	\
	-R .note.netbsd.ident
.endif

#
# Targets to check if DESTDIR or RELEASEDIR is provided
#
.if !target(check_DESTDIR)
check_DESTDIR: .PHONY .NOTMAIN
.if !defined(DESTDIR)
	@echo "setenv DESTDIR before doing that!"
	@false
.else
	@true
.endif
.endif

.if !target(check_RELEASEDIR)
check_RELEASEDIR: .PHONY .NOTMAIN
.if !defined(RELEASEDIR)
	@echo "setenv RELEASEDIR before doing that!"
	@false
.else
	@true
.endif
.endif

#
# Build a dynamically linked /bin and /sbin, with the necessary shared
# libraries moved from /usr/lib to /lib and the shared linker moved
# from /usr/libexec to /lib
#
# Note that if the BINDIR is not /bin or /sbin, then we always use the
# non-DYNAMICROOT behavior (i.e. it is only enabled for programs in /bin
# and /sbin).  See <bsd.shlib.mk>.
#
MKDYNAMICROOT?=	yes

#
# Where the system object and source trees are kept; can be configurable
# by the user in case they want them in ~/foosrc and ~/fooobj (for example).
#
BSDSRCDIR?=	/usr/src
BSDOBJDIR?=	/usr/obj
NETBSDSRCDIR?=	${BSDSRCDIR}

BINGRP?=	wheel
BINOWN?=	root
BINMODE?=	555
NONBINMODE?=	444

# These are here mainly because we don't want suid root in case
# a Makefile defines BINMODE.
RUMPBINGRP?=	wheel
RUMPBINOWN?=	root
RUMPBINMODE?=	555
RUMPNONBINMODE?=444

MANDIR?=	/usr/share/man
MANGRP?=	wheel
MANOWN?=	root
MANMODE?=	${NONBINMODE}
MANINSTALL?=	${_MANINSTALL}

INFODIR?=	/usr/share/info
INFOGRP?=	wheel
INFOOWN?=	root
INFOMODE?=	${NONBINMODE}

LIBDIR?=	/usr/lib

LINTLIBDIR?=	/usr/libdata/lint
LIBGRP?=	${BINGRP}
LIBOWN?=	${BINOWN}
LIBMODE?=	${NONBINMODE}

DOCDIR?=	/usr/share/doc
HTMLDOCDIR?=	/usr/share/doc/html
DOCGRP?=	wheel
DOCOWN?=	root
DOCMODE?=	${NONBINMODE}

NLSDIR?=	/usr/share/nls
NLSGRP?=	wheel
NLSOWN?=	root
NLSMODE?=	${NONBINMODE}

KMODULEGRP?=	wheel
KMODULEOWN?=	root
KMODULEMODE?=	${NONBINMODE}

LOCALEDIR?=	/usr/share/locale
LOCALEGRP?=	wheel
LOCALEOWN?=	root
LOCALEMODE?=	${NONBINMODE}

FIRMWAREDIR?=	/libdata/firmware
FIRMWAREGRP?=	wheel
FIRMWAREOWN?=	root
FIRMWAREMODE?=	${NONBINMODE}

DEBUGDIR?=	/usr/libdata/debug
DEBUGGRP?=	wheel
DEBUGOWN?=	root
DEBUGMODE?=	${NONBINMODE}

#
# Data-driven table using make variables to control how
# toolchain-dependent targets and shared libraries are built
# for different platforms and object formats.
#
# OBJECT_FMT:		currently either "ELF" or "a.out".
#
# All platforms are ELF.
#
OBJECT_FMT=	ELF

#
# If this platform's toolchain is missing, we obviously cannot build it.
#
.if ${TOOLCHAIN_MISSING} != "no"
MKBINUTILS:= no
MKGDB:= no
MKGCC:= no
.endif

#
# If we are using an external toolchain, we can still build the target's
# binutils, but we cannot build GCC's support libraries, since those are
# tightly-coupled to the version of GCC being used.
#
.if defined(EXTERNAL_TOOLCHAIN)
MKGCC:= no
.endif

#
# The m68000 port is incomplete.
#
.if ${MACHINE_ARCH} == "m68000"
NOPIC=		# defined
MKISCSI=	no
# XXX GCC 4 outputs mcount() calling sequences that try to load values
# from over 64KB away and this fails to assemble.
.if defined(HAVE_GCC)
NOPROFILE=	# defined
.endif
.endif

#
# The ia64 port is incomplete.
#
.if ${MACHINE_ARCH} == "ia64"
MKLINT=		no
MKGDB=		no
.endif

#
# On the MIPS, all libs are compiled with ABIcalls (and are thus PIC),
# not just shared libraries, so don't build the _pic version.
#
.if ${MACHINE_ARCH} == "mipsel" || ${MACHINE_ARCH} == "mipseb" || \
    ${MACHINE_ARCH} == "mips64el" || ${MACHINE_ARCH} == "mips64eb"
MKPICLIB:=	no
.endif

#
# On VAX using ELF, all objects are PIC, not just shared libraries,
# so don't build the _pic version.
#
.if ${MACHINE_ARCH} == "vax"
MKPICLIB=	no
.endif

#
# Location of the file that contains the major and minor numbers of the
# version of a shared library.  If this file exists a shared library
# will be built by <bsd.lib.mk>.
#
SHLIB_VERSION_FILE?= ${.CURDIR}/shlib_version

#
# GNU sources and packages sometimes see architecture names differently.
#
GNU_ARCH.coldfire=m68k
# MINIX/arm default
GNU_ARCH.earm=arm
GCC_CONFIG_ARCH.earm=armv7-a
GNU_ARCH.earmeb=armeb
# MINIX/intel default
GNU_ARCH.i386=i586
GCC_CONFIG_ARCH.i386=i586
GCC_CONFIG_TUNE.i386=nocona
GCC_CONFIG_TUNE.x86_64=nocona
GNU_ARCH.m68000=m68010
GNU_ARCH.sh3eb=sh
GNU_ARCH.sh3el=shle
GNU_ARCH.mips64eb=mips64
MACHINE_GNU_ARCH=${GNU_ARCH.${MACHINE_ARCH}:U${MACHINE_ARCH}}

#
# In order to identify NetBSD to GNU packages, we sometimes need
# an "elf" tag for historically a.out platforms.
#
.if ${MACHINE_ARCH} == "earm" || ${MACHINE_ARCH} == "earmeb"
MACHINE_GNU_PLATFORM?=${MACHINE_GNU_ARCH}--netbsdelf-eabi
.elif (${MACHINE_GNU_ARCH} == "arm" || \
     ${MACHINE_GNU_ARCH} == "armeb" || \
     ${MACHINE_ARCH} == "i386" || \
     ${MACHINE_CPU} == "m68k" || \
     ${MACHINE_GNU_ARCH} == "sh" || \
     ${MACHINE_GNU_ARCH} == "shle" || \
     ${MACHINE_ARCH} == "sparc" || \
     ${MACHINE_ARCH} == "vax")
MACHINE_GNU_PLATFORM?=${MACHINE_GNU_ARCH}--netbsdelf
.else
MACHINE_GNU_PLATFORM?=${MACHINE_GNU_ARCH}--netbsd
.endif

.if defined(__MINIX)
# We have a simpler toolchain naming scheme
MACHINE_GNU_PLATFORM:=${MACHINE_GNU_ARCH}-elf32-minix
.endif # defined(__MINIX)

#
# Determine if arch uses native kernel modules with rump
#
.if ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "x86_64"
RUMPKMOD=	# defined
.endif

TARGETS+=	all clean cleandir depend dependall includes \
		install lint obj regress tags html analyze
PHONY_NOTMAIN =	all clean cleandir depend dependall distclean includes \
		install lint obj regress beforedepend afterdepend \
		beforeinstall afterinstall realinstall realdepend realall \
		html subdir-all subdir-install subdir-depend analyze
.PHONY:		${PHONY_NOTMAIN}
.NOTMAIN:	${PHONY_NOTMAIN}

.if ${NEED_OWN_INSTALL_TARGET} != "no"
.if !target(install)
install:	beforeinstall .WAIT subdir-install realinstall .WAIT afterinstall
beforeinstall:
subdir-install:
realinstall:
afterinstall:
.endif
all:		realall subdir-all
subdir-all:
realall:
depend:		realdepend subdir-depend
subdir-depend:
realdepend:
distclean:	cleandir
cleandir:	clean

dependall:	.NOTMAIN realdepend .MAKE
	@cd "${.CURDIR}"; ${MAKE} realall
.endif

#
# Define MKxxx variables (which are either yes or no) for users
# to set in /etc/mk.conf and override in the make environment.
# These should be tested with `== "no"' or `!= "no"'.
# The NOxxx variables should only be set by Makefiles.
#
# Please keep etc/Makefile and share/man/man5/mk.conf.5 in sync
# with changes to the MK* variables here.
#

#
# Supported NO* options (if defined, MK* will be forced to "no",
# regardless of user's mk.conf setting).
#
# Source makefiles should set NO*, and not MK*, and must do so before
# including bsd.own.mk.
#
.for var in \
	NOCRYPTO NODOC NOHTML NOINFO NOLINKLIB NOLINT NOMAN NONLS NOOBJ NOPIC \
	NOPICINSTALL NOPROFILE NOSHARE NOSTATICLIB
.if defined(${var})
MK${var:S/^NO//}:=	no
.endif
.endfor

#
# Older-style variables that enabled behaviour when set.
#
.for var in MANZ UNPRIVED UPDATE
.if defined(${var})
MK${var}:=	yes
.endif
.endfor

#
# MK* options which have variable defaults.
#
.if ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "sparc64" || \
    ${MACHINE_ARCH} == "mips64eb" || ${MACHINE_ARCH} == "mips64el"
MKCOMPAT?=	yes
.else
# Don't let this build where it really isn't supported.
MKCOMPAT:=	no
.endif

#.if ${MACHINE_ARCH} == "x86_64" || ${MACHINE_ARCH} == "i386" || \

.if ${MACHINE} == "evbppc"
MKCOMPATMODULES?=	yes
.else
MKCOMPATMODULES:=	no
.endif

#
# Default mips64 to softfloat now.
# emips is always softfloat.
#
.if ${MACHINE_ARCH} == "mips64eb" || ${MACHINE_ARCH} == "mips64el" || \
    ${MACHINE} == "emips"
MKSOFTFLOAT?=	yes
.endif

.if ${MACHINE} == "emips"
SOFTFLOAT_BITS=	32
.endif

.if ${MACHINE_ARCH} == "i386" || \
    ${MACHINE_ARCH} == "x86_64" || \
    ${MACHINE_ARCH} == "sparc" 
MKSLJIT?=	yes
.else
MKSLJIT?=	no
.endif

#
# MK* backward compatibility.
#
.if defined(MKBFD)
MKBINUTILS?=	${MKBFD}
.endif

#
# We want to build zfs only for i386 and amd64 by default for now.
#
.if ${MACHINE} == "amd64" || ${MACHINE} == "i386"
MKZFS?=		yes
.endif

# Some tough Minix defaults
MKCOVERAGE?=	no
MKPROFILE?=	no
MKSTATICLIB:=	yes
MKLINT:=	no

# LSC MINIX does not support these features ATM.
USE_FORT:=	no
MKYP:=		no
MKPF:=		no
MKNLS:=		no
MKHESIOD:=	no
MKPOSTFIX:=	no
MKKMOD:=	no
MKEXTSRC:=	no
MKRUMP:=	no
MKSKEY:=	no
MKCRYPTO:=	no
MKMDNS:=	no
MKNPF:=		no
MKISCSI:=	no
MKLVM:=		no
MKKERBEROS:=	no
MKLDAP:=	no
MKPAM:=		no
MKIPFILTER:=	no
MKINET6:=	no
MKGROFF:=	no
MKHTML:=	no

#
# MK* options which default to "yes".
#
_MKVARS.yes= \
	MKATF \
	MKBINUTILS \
	MKCRYPTO MKCOMPLEX MKCVS MKCXX \
	MKDOC \
	MKGCC MKGCCCMDS MKGDB MKGROFF \
	MKHESIOD MKHTML \
	MKIEEEFP MKINET6 MKINFO MKIPFILTER MKISCSI \
	MKKERBEROS \
	MKKMOD \
	MKLDAP MKLINKLIB MKLINT MKLVM \
	MKMAN MKMANDOC \
	MKMDNS \
	MKMAKEMANDB \
	MKNLS \
	MKNPF \
	MKOBJ \
	MKPAM MKPERFUSE \
	MKPF MKPIC MKPICINSTALL MKPICLIB MKPOSTFIX MKPROFILE \
	MKRUMP \
	MKSHARE MKSKEY MKSTATICLIB \
	MKX11FONTS \
	MKYP

#MINIX-specific vars
_MKVARS.yes+= \
	MKMCONTEXT MKSYSDEBUG MKLIVEUPDATE MKSTATECTL MKTRACE MKLWIP
.if (${MACHINE_ARCH} == "i386")
_MKVARS.yes+= \
	MKWATCHDOG MKACPI MKAPIC MKDEBUGREG MKINSTALLBOOT MKPCI
.endif

.for var in ${_MKVARS.yes}
${var}?=	yes
.endfor

#
# Exceptions to the above:
#
#.if ${MACHINE} == "evbppc"
#MKKMOD=		no
#.endif

#
# MK* options which default to "no".  Note that MKZFS has a different
# default for some platforms, see above.
#
_MKVARS.no= \
	MKBSDGREP MKBSDTAR \
	MKCATPAGES MKCRYPTO_RC5 MKDEBUG \
	MKDEBUGLIB MKDTRACE MKEXTSRC \
	MKKYUA \
	MKMANZ MKOBJDIRS \
	MKLLVM MKPCC \
	MKPIGZGZIP \
	MKREPRO \
	MKSOFTFLOAT MKSTRIPIDENT \
	MKUNPRIVED MKUPDATE MKX11 MKZFS

#MINIX-specific vars
_MKVARS.no+= \
	MKIMAGEONLY MKSMALL MKBITCODE
.if (${MACHINE_ARCH} == "earm")
_MKVARS.no+= \
	MKWATCHDOG MKACPI MKAPIC MKDEBUGREG MKINSTALLBOOT MKPCI
.endif

.for var in ${_MKVARS.no}
${var}?=no
.endfor

#
# Do we default to XFree86 or Xorg for this platform?
#
.if \
    ${MACHINE} == "acorn32"	|| \
    ${MACHINE} == "alpha"	|| \
    ${MACHINE} == "amiga"	|| \
    ${MACHINE} == "ews4800mips"	|| \
    ${MACHINE} == "mac68k"	|| \
    ${MACHINE} == "newsmips"	|| \
    ${MACHINE} == "pmax"	|| \
    ${MACHINE} == "sun3"	|| \
    ${MACHINE} == "x68k"
X11FLAVOUR?=	XFree86
.else
X11FLAVOUR?=	Xorg
.endif

#
# Force some options off if their dependencies are off.
#

.if ${MKCXX} == "no"
MKATF:=		no
MKGROFF:=	no
MKKYUA:=	no
.endif

.if ${MKCRYPTO} == "no"
MKKERBEROS:=	no
MKLDAP:=	no
.endif

.if ${MKMAN} == "no"
MKCATPAGES:=	no
MKHTML:=	no
.endif

_MANINSTALL=	maninstall
.if ${MKCATPAGES} != "no"
_MANINSTALL+=	catinstall
.endif
.if ${MKHTML} != "no"
_MANINSTALL+=	htmlinstall
.endif

.if ${MKLINKLIB} == "no"
MKLINT:=	no
MKPICINSTALL:=	no
MKPROFILE:=	no
.endif

.if ${MKPIC} == "no"
MKPICLIB:=	no
.endif

.if ${MKOBJ} == "no"
MKOBJDIRS:=	no
.endif

.if ${MKSHARE} == "no"
MKCATPAGES:=	no
MKDOC:=		no
MKINFO:=	no
MKHTML:=	no
MKMAN:=		no
MKNLS:=		no
.endif

# MINIX
.if ${MKSMALL} == "yes"
MKWATCHDOG:=	no
MKACPI:=	no
MKAPIC:=	no
MKMCONTEXT:=	no
MKDEBUGREG:=	no
MKSYSDEBUG:=	no
MKLIVEUPDATE:=	no
MKSTATECTL:=	no
MKTRACE:=	no
.endif

#
# install(1) parameters.
#
COPY?=		-c
.if ${MKUPDATE} == "no"
PRESERVE?=	
.else
PRESERVE?=	-p
.endif
RENAME?=	-r
HRDLINK?=	-l h
SYMLINK?=	-l s

METALOG?=	${DESTDIR}/METALOG
METALOG.add?=	${TOOL_CAT} -l >> ${METALOG}
.if (${_SRC_TOP_} != "")	# only set INSTPRIV if inside ${NETBSDSRCDIR}
.if ${MKUNPRIVED} != "no"
INSTPRIV.unpriv=-U -M ${METALOG} -D ${DESTDIR} -h sha256
.else
INSTPRIV.unpriv=
.endif
INSTPRIV?=	${INSTPRIV.unpriv} -N ${NETBSDSRCDIR}/etc
.endif
STRIPFLAG?=	

.if ${NEED_OWN_INSTALL_TARGET} != "no"
INSTALL_DIR?=		${INSTALL} ${INSTPRIV} -d
INSTALL_FILE?=		${INSTALL} ${INSTPRIV} ${COPY} ${PRESERVE} ${RENAME}
INSTALL_LINK?=		${INSTALL} ${INSTPRIV} ${HRDLINK} ${RENAME}
INSTALL_SYMLINK?=	${INSTALL} ${INSTPRIV} ${SYMLINK} ${RENAME}
HOST_INSTALL_FILE?=	${INSTALL} ${COPY} ${PRESERVE} ${RENAME}
HOST_INSTALL_DIR?=	${INSTALL} -d
HOST_INSTALL_SYMLINK?=	${INSTALL} ${SYMLINK} ${RENAME}
.endif

#
# Set defaults for the USE_xxx variables.
#

#
# USE_* options which default to "no" and will be forced to "no" if their
# corresponding MK* variable is set to "no".
#
.for var in USE_SKEY
.if (${${var:S/USE_/MK/}} == "no")
${var}:= no
.else
${var}?= no
.endif
.endfor

#
# USE_* options which default to "yes" unless their corresponding MK*
# variable is set to "no".
#
.for var in USE_HESIOD USE_INET6 USE_KERBEROS USE_LDAP USE_PAM USE_YP
.if (${${var:S/USE_/MK/}} == "no")
${var}:= no
.else
${var}?= yes
.endif
.endfor

#MINIX-specific vars
.for var in \
	USE_WATCHDOG USE_ACPI USE_APIC USE_MCONTEXT USE_DEBUGREG USE_SYSDEBUG \
	USE_LIVEUPDATE USE_STATECTL USE_TRACE USE_PCI USE_BITCODE
.if (${${var:S/USE_/MK/}} == "no")
${var}:= no
.else
${var}?= yes
.endif
.endfor

#
# USE_* options which default to "yes".
#
.for var in USE_JEMALLOC
${var}?= yes
.endfor

#
# USE_* options which default to "no".
#
# For now, disable pigz as compressor by default
.for var in USE_PIGZGZIP USE_LIBTRE
${var}?= no
.endfor

.if ${USE_PIGZGZIP} != "no"
TOOL_GZIP=		${TOOL_PIGZ}
.else
TOOL_GZIP=		gzip
.endif

#
# Where X11 sources are and where it is installed to.
#
.if !defined(X11SRCDIR)
.if exists(${NETBSDSRCDIR}/../xsrc)
X11SRCDIR!=		cd "${NETBSDSRCDIR}/../xsrc" && pwd
.else
X11SRCDIR=		/usr/xsrc
.endif
.endif # !defined(X11SRCDIR)

X11SRCDIR.xc?=		${X11SRCDIR}/xfree/xc
X11SRCDIR.local?=	${X11SRCDIR}/local
.if ${X11FLAVOUR} == "Xorg"
X11ROOTDIR?=		/usr/X11R7
.else
X11ROOTDIR?=		/usr/X11R6
.endif
X11BINDIR?=		${X11ROOTDIR}/bin
X11ETCDIR?=		/etc/X11
X11FONTDIR?=		${X11ROOTDIR}/lib/X11/fonts
X11INCDIR?=		${X11ROOTDIR}/include
X11LIBDIR?=		${X11ROOTDIR}/lib/X11
X11MANDIR?=		${X11ROOTDIR}/man
X11SHAREDIR?=		${X11ROOTDIR}/share
X11USRLIBDIR?=		${X11ROOTDIR}/lib

#
# New modular-xorg based builds
#
X11SRCDIRMIT?=		${X11SRCDIR}/external/mit
.for _lib in \
	FS ICE SM X11 XScrnSaver XTrap Xau Xcomposite Xcursor Xdamage \
	Xdmcp Xevie Xext Xfixes Xfont Xft Xi Xinerama Xmu Xpm \
	Xrandr Xrender Xres Xt Xtst Xv XvMC Xxf86dga Xxf86misc Xxf86vm drm \
	fontenc xkbfile xkbui Xaw lbxutil Xfontcache pciaccess xcb
X11SRCDIR.${_lib}?=		${X11SRCDIRMIT}/lib${_lib}/dist
.endfor

.for _proto in \
	xcmisc xext xf86bigfont bigreqs input kb x fonts fixes scrnsaver \
	xinerama dri2 render resource record video xf86dga xf86misc \
	xf86vidmode composite damage trap gl randr fontcache xf86dri \
	xcb-
X11SRCDIR.${_proto}proto?=		${X11SRCDIRMIT}/${_proto}proto/dist
.endfor

.for _dir in \
	xtrans fontconfig expat freetype evieext mkfontscale bdftopcf \
	xkbcomp xorg-cf-files imake xorg-server xbiff xkbdata xkeyboard-config \
	xbitmaps appres xeyes xev xedit sessreg pixman \
	beforelight bitmap editres makedepend fonttosfnt fslsfonts \
	fstobdf MesaDemos MesaGLUT MesaLib ico iceauth lbxproxy listres lndir \
	luit xproxymanagementprotocol mkfontdir oclock proxymngr rgb \
	setxkbmap smproxy twm viewres x11perf xauth xcalc xclipboard \
	xclock xcmsdb xconsole xcutsel xditview xdpyinfo xdriinfo xdm \
	xfd xf86dga xfindproxy xfontsel xfwp xgamma xgc xhost xinit \
	xkill xload xlogo xlsatoms xlsclients xlsfonts xmag xmessage \
	xmh xmodmap xmore xman xprop xrandr xrdb xrefresh xset \
	xsetmode xsetpointer xsetroot xsm xstdcmap xvidtune xvinfo \
	xwininfo xwud xprehashprinterlist xplsprinters xkbprint xkbevd \
	xterm xwd xfs xfsinfo xphelloworld xtrap xkbutils xkbcomp \
	xkeyboard-config xinput xcb-util \
	font-adobe-100dpi font-adobe-75dpi font-adobe-utopia-100dpi \
	font-adobe-utopia-75dpi font-adobe-utopia-type1 \
	font-alias \
	font-bh-100dpi font-bh-75dpi font-bh-lucidatypewriter-100dpi \
	font-bh-lucidatypewriter-75dpi font-bh-ttf font-bh-type1 \
	font-bitstream-100dpi font-bitstream-75dpi font-bitstream-type1 \
	font-cursor-misc font-daewoo-misc font-dec-misc font-ibm-type1 \
	font-isas-misc font-jis-misc font-misc-misc font-mutt-misc \
	font-sony-misc font-util ttf-bitstream-vera encodings
X11SRCDIR.${_dir}?=		${X11SRCDIRMIT}/${_dir}/dist
.endfor

.for _i in \
	elographics keyboard mouse synaptics vmmouse void ws
X11SRCDIR.xf86-input-${_i}?=	${X11SRCDIRMIT}/xf86-input-${_i}/dist
.endfor

.for _v in \
	ag10e apm ark ast ati chips cirrus crime \
	geode glint i128 i740 igs imstt intel mach64 mga \
	neomagic newport nsc nv nvxbox openchrome pnozz \
	r128 radeonhd rendition \
	s3 s3virge savage siliconmotion sis suncg14 \
	suncg6 sunffb sunleo suntcx \
	tdfx tga trident tseng vesa vga via vmware wsfb xgi
X11SRCDIR.xf86-video-${_v}?=	${X11SRCDIRMIT}/xf86-video-${_v}/dist
.endfor

.if ${X11FLAVOUR} == "Xorg"
X11DRI?=			yes
.endif

X11DRI?=			no
X11LOADABLE?=			yes


#
# Where extsrc sources are and where it is installed to.
#
.if !defined(EXTSRCSRCDIR)
.if exists(${NETBSDSRCDIR}/../extsrc)
EXTSRCSRCDIR!=		cd "${NETBSDSRCDIR}/../extsrc" && pwd
.else
EXTSRCSRCDIR=		/usr/extsrc
.endif
.endif # !defined(EXTSRCSRCDIR)

EXTSRCROOTDIR?=		/usr/ext
EXTSRCBINDIR?=		${EXTSRCROOTDIR}/bin
EXTSRCETCDIR?=		/etc/ext
EXTSRCINCDIR?=		${EXTSRCROOTDIR}/include
EXTSRCLIBDIR?=		${EXTSRCROOTDIR}/lib/ext
EXTSRCMANDIR?=		${EXTSRCROOTDIR}/man
EXTSRCUSRLIBDIR?=	${EXTSRCROOTDIR}/lib

#
# MAKEDIRTARGET dir target [extra make(1) params]
#	run "cd $${dir} && ${MAKEDIRTARGETENV} ${MAKE} [params] $${target}", with a pretty message
#
MAKEDIRTARGETENV?=
MAKEDIRTARGET=\
	@_makedirtarget() { \
		dir="$$1"; shift; \
		target="$$1"; shift; \
		case "$${dir}" in \
		/*)	this="$${dir}/"; \
			real="$${dir}" ;; \
		.)	this="${_THISDIR_}"; \
			real="${.CURDIR}" ;; \
		*)	this="${_THISDIR_}$${dir}/"; \
			real="${.CURDIR}/$${dir}" ;; \
		esac; \
		show=$${this:-.}; \
		echo "$${target} ===> $${show%/}$${1:+	(with: $$@)}"; \
		cd "$${real}" \
		&& ${MAKEDIRTARGETENV} ${MAKE} _THISDIR_="$${this}" "$$@" $${target}; \
	}; \
	_makedirtarget

#
# MAKEVERBOSE support.  Levels are:
#	0	Minimal output ("quiet")
#	1	Describe what is occurring
#	2	Describe what is occurring and echo the actual command
#	3	Ignore the effect of the "@" prefix in make commands
#	4	Trace shell commands using the shell's -x flag
#		
MAKEVERBOSE?=		2

.if ${MAKEVERBOSE} == 0
_MKMSG?=	@\#
_MKSHMSG?=	: echo
_MKSHECHO?=	: echo
.SILENT:
.elif ${MAKEVERBOSE} == 1
_MKMSG?=	@echo '   '
_MKSHMSG?=	echo '   '
_MKSHECHO?=	: echo
.SILENT:
.else	# MAKEVERBOSE >= 2
_MKMSG?=	@echo '\#  '
_MKSHMSG?=	echo '\#  '
_MKSHECHO?=	echo
.SILENT: __makeverbose_dummy_target__
.endif	# MAKEVERBOSE >= 2
.if ${MAKEVERBOSE} >= 3
.MAKEFLAGS:	-dl
.endif	# ${MAKEVERBOSE} >= 3
.if ${MAKEVERBOSE} >= 4
.MAKEFLAGS:	-dx
.endif	# ${MAKEVERBOSE} >= 4

_MKMSG_BUILD?=		${_MKMSG} "  build "
_MKMSG_CREATE?=		${_MKMSG} " create "
_MKMSG_COMPILE?=	${_MKMSG} "compile "
_MKMSG_FORMAT?=		${_MKMSG} " format "
_MKMSG_INSTALL?=	${_MKMSG} "install "
_MKMSG_LINK?=		${_MKMSG} "   link "
_MKMSG_LEX?=		${_MKMSG} "    lex "
_MKMSG_REMOVE?=		${_MKMSG} " remove "
_MKMSG_YACC?=		${_MKMSG} "   yacc "

_MKSHMSG_CREATE?=	${_MKSHMSG} " create "
_MKSHMSG_INSTALL?=	${_MKSHMSG} "install "

_MKTARGET_BUILD?=	${_MKMSG_BUILD} ${.CURDIR:T}/${.TARGET}
_MKTARGET_CREATE?=	${_MKMSG_CREATE} ${.CURDIR:T}/${.TARGET}
_MKTARGET_COMPILE?=	${_MKMSG_COMPILE} ${.CURDIR:T}/${.TARGET}
_MKTARGET_FORMAT?=	${_MKMSG_FORMAT} ${.CURDIR:T}/${.TARGET}
_MKTARGET_INSTALL?=	${_MKMSG_INSTALL} ${.TARGET}
_MKTARGET_LINK?=	${_MKMSG_LINK} ${.CURDIR:T}/${.TARGET}
_MKTARGET_LEX?=		${_MKMSG_LEX} ${.CURDIR:T}/${.TARGET}
_MKTARGET_REMOVE?=	${_MKMSG_REMOVE} ${.TARGET}
_MKTARGET_YACC?=	${_MKMSG_YACC} ${.CURDIR:T}/${.TARGET}

.if ${MKMANDOC} == "yes"
TARGETS+=	lintmanpages
.endif

TESTSBASE=	/usr/tests

.endif	# !defined(_BSD_OWN_MK_)
