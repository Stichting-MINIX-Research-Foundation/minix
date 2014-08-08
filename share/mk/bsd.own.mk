#	$NetBSD: bsd.own.mk,v 1.755 2013/10/29 16:11:46 joerg Exp $

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

MKKYUA?=	yes

MKMCLINKER?=	no
MKCLANGRT?=	no
MKGCC?=		no
MKGCCCMDS?=	no
MKPROFILE?=	no
MKSLJIT?=	no

#MINIX-specific variables
MKCOVERAGE?=	no

# LSC MINIX does not support these features ATM.
USE_FORT:=	no
USE_SSP:=	no
MKCRYPTO:=	no
MKGDB:=		no
MKGROFF:=	no
MKHESIOD:=	no
MKHTML:=	no
MKINET6:=	no
MKIPFILTER:=	no
MKISCSI:=	no
MKKERBEROS:=	no
MKKMOD:=	no
MKLDAP:=	no
MKLINT:=	no
MKLVM:=		no
MKMDNS:=	no
MKNLS:=		no
MKNPF:=		no
MKPAM:=		no
MKPF:=		no
MKPOSTFIX:=	no
MKRUMP:=	no
MKSKEY:=	no
MKYP:=		no

WEAKALIASOVERRIDEPASS?=${NETBSDSRCDIR}/minix/llvm/bin/weak-alias-module-override.so
GOLD_PLUGIN?=${NETBSDSRCDIR}/minix/llvm/bin/LLVMgold.so

# By default when running LLVM passes:
#  -  do not run optimization while running LLVM passes
#  -  run the passe which allows assembly weak symbols to be
#     overriden by bitcode functions
OPTFLAGS?=	-disable-opt \
		-disable-internalize -disable-inlining \
		-load ${WEAKALIASOVERRIDEPASS} -weak-alias-module-override

# Whitout -Wl,--no-ctors-in-init-array, golds moves the constructors out of
# .ctors into .init_array, which is bad on intel.
BITCODE_LD_FLAGS_1ST?= \
		-Wl,--no-ctors-in-init-array \
		-Wl,-plugin=${GOLD_PLUGIN} \
		-Wl,-plugin-opt=-disable-opt \
		-Wl,-plugin-opt=-disable-inlining

BITCODE_LD_FLAGS_2ND?=${BITCODE_LD_FLAGS_1ST}

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
# LSC: We have to use a temporary variable, as some test are done simply on the
#      definition (ifdef) of it.
.    if ${_HAVE_LLVM:U} == ""
       _HAVE_LLVM!= (exec 2>&1; clang --version || echo "")
       _HAVE_LLVM:= ${_HAVE_LLVM:M[0-9]\.[0-9]}
.      if ${_HAVE_LLVM} != ""
         HAVE_LLVM?= ${_HAVE_LLVM}
.      endif
.    endif # ${_HAVE_LLVM:U} == ""

# If DESTDIR was specified, and we are not using the tools, then make sure to
# build out-of-tree and to refer only DESTDIR for target binaries
# The case when using tools is already handled below.
.    if ${DESTDIR:U} != ""
        CPPFLAGS+= --sysroot=${DESTDIR}
        LDFLAGS+= --sysroot=${DESTDIR}
.    endif # ${DESTDIR:U} != ""

     MKTOOLS?=no
.  endif # ${USETOOLS:Uno} != "yes"

.  if !defined(HOSTPROG) && !defined(HOSTLIB)
# LSC FIXME: Override MACHINE as the native minix host make command will set 
#            it to i686.
.    if ${MACHINE_ARCH} == "i386"
       MACHINE:= i386
.    endif
# LSC FIXME: On a native ARM system MACHINE_ARCH is earmv7 instead of earm...
.    if !empty(${MACHINE_ARCH:Mearm*})
       MACHINE_ARCH:= earm
.    endif
.  endif # !defined(HOSTPROG) && !defined(HOSTLIB)
.endif # __uname_s == "Minix"

.if ${HAVE_GCC:Dyes} == "yes" || \
    (${MKGCCCMDS:Uno} == "yes" && ${MKLLVM:Uyes} == "no")
# We are building with GCC, means we cannot build LIBCXX, and need LIBSTDCXX
MKLIBCXX?=	no # Can't compile with GCC.

.  if ${USETOOLS:Uno} == "yes" || ${MKLIBSTDCXX:Uno} == "yes"
# When cross-compiling, or building MKLIBSTDCXX, use the in-tree LIBSTDCXX
MKLIBSTDCXX?=	yes
CXXFLAGS+=	-I${DESTDIR}/usr/include/g++
.  endif # ${USETOOLS:Uno} == "yes"

.  if ${MKLIBSTDCXX:Uno} == "no"
# Specify the C++ system header for the host libstdc++, as we are not building
# LIBSTDCXX
CXXFLAGS+=	-I/usr/include/g++
.  endif # ${MKLIBSTDCXX:Uno} == "no"

.endif # ${HAVE_GCC:Dyes} == "yes" || \
       # (${MKGCCCMDS:Uno} == "yes" && ${MKLLVM:Uyes} == "no")

# MKGCCCMDS == yes implies MKGCC == yes
.if ${MKGCCCMDS} == "yes"
MKGCC=		yes
MKBINUTILS?=	yes # We are installing GCC, so trigger binutils.
MKLIBSTDCXX?=	yes # Also install the GNU C++ library
.endif # ${MKGCCCMDS} == "yes"

# The default value of MKLIBSTDCXX cannot be set before the previous test.
MKLIBSTDCXX?=	no

.if ${MKGCC} == "yes"
HAVE_GCC?=	45 # The in-tree gcc version is 4.5
.endif # ${MKGCC} == "yes"

.if ${USETOOLS:Uno} == "yes"
MKLLVM?=	yes
.endif # ${USETOOLS:Uno} == "yes"

.if ${MKLLVM:Uno} == "yes"
HAVE_LLVM?=	34 # The in-tree LLVM version is 3.4
MKBINUTILS?=	yes # We are installing clang, so trigger binutils.
.endif # ${MKLLVM:Uno} == "yes"

.if ${HAVE_LLVM:Dyes} == "yes"
HAVE_LIBGCC?=	no
.endif # ${HAVE_LLVM:Dyes} == "yes"

# The default value has to be set after we have figured out if we are using GCC
# or not.
MKLIBCXX?=	yes # Build by default libc++

# The default value of MKBINUTILS cannot be set before the previous test.
MKBINUTILS?=	no

.endif # defined(__MINIX)

MAKECONF?=	/etc/mk.conf
.-include "${MAKECONF}"

#
# CPU model, derived from MACHINE_ARCH
#
MACHINE_CPU=	${MACHINE_ARCH:C/mipse[bl]/mips/:C/mips64e[bl]/mips/:C/sh3e[bl]/sh3/:S/coldfire/m68k/:S/m68000/m68k/:C/arm.*/arm/:C/earm.*/arm/:S/earm/arm/:S/powerpc64/powerpc/}

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


.if ${MACHINE_ARCH} == "ia64"
USE_COMPILERCRTSTUFF?=	yes
.else
USE_COMPILERCRTSTUFF?=	no
.endif

.if ${MKLLVM:Uno} == "yes" && (${MACHINE_ARCH} == "i386" || ${MACHINE_ARCH} == "x86_64")
HAVE_LIBGCC?=	no
.else
HAVE_LIBGCC?=	yes
.endif

HAVE_GDB?=	7

.if (${MACHINE_ARCH} == "alpha") || \
    (${MACHINE_ARCH} == "hppa") || \
    (${MACHINE_ARCH} == "ia64") || \
    (${MACHINE_CPU} == "mips")
HAVE_SSP?=	no
.else
HAVE_SSP?=	yes
.if ${USE_FORT:Uno} != "no"
USE_SSP?=	yes
.endif
.endif

.if empty(.MAKEFLAGS:tW:M*-V .OBJDIR*)
.if defined(MAKEOBJDIRPREFIX) || defined(MAKEOBJDIR)
PRINTOBJDIR=	${MAKE} -r -V .OBJDIR -f /dev/null xxx
.else
PRINTOBJDIR=	${MAKE} -V .OBJDIR
.endif
.else
PRINTOBJDIR=	echo /error/bsd.own.mk/PRINTOBJDIR # avoid infinite recursion
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
# If _SRC_TOP_ != "", we're within the NetBSD source tree.
# * Set defaults for NETBSDSRCDIR and _SRC_TOP_OBJ_.
# * Define _NETBSD_VERSION_DEPENDS.  Targets that depend on the
#   NetBSD version, or on variables defined at build time, can
#   declare a dependency on ${_NETBSD_VERSION_DEPENDS}.
#
.if (${_SRC_TOP_} != "")		# {

NETBSDSRCDIR?=	${_SRC_TOP_}

.if !defined(_SRC_TOP_OBJ_)
_SRC_TOP_OBJ_!=		cd "${_SRC_TOP_}" && ${PRINTOBJDIR}
.MAKEOVERRIDES+=	_SRC_TOP_OBJ_
.endif

_NETBSD_VERSION_DEPENDS=	${_SRC_TOP_OBJ_}/params
_NETBSD_VERSION_DEPENDS+=	${NETBSDSRCDIR}/sys/sys/param.h
_NETBSD_VERSION_DEPENDS+=	${NETBSDSRCDIR}/sys/conf/newvers.sh
_NETBSD_VERSION_DEPENDS+=	${NETBSDSRCDIR}/sys/conf/osrelease.sh
${_SRC_TOP_OBJ_}/params: .NOTMAIN .OPTIONAL # created by top level "make build"

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

TOOL_CC.clang=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-clang
TOOL_CPP.clang=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-clang-cpp
TOOL_CXX.clang=		${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-clang++
TOOL_OBJC.clang=	${EXTERNAL_TOOLCHAIN}/bin/${MACHINE_GNU_PLATFORM}-clang
TOOL_OPT.clang=		${EXTERNAL_TOOLCHAIN}/bin/opt
TOOL_LLC.clang=		${EXTERNAL_TOOLCHAIN}/bin/llc
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
TOOL_OPT.clang=		${TOOLDIR}/bin/opt
TOOL_LLC.clang=		${TOOLDIR}/bin/llc

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
HOST_MKDEPCXX=	${TOOLDIR}/bin/${_TOOL_PREFIX}host-mkdep

DBSYM=		${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-dbsym
ELF2AOUT=	${TOOLDIR}/bin/${_TOOL_PREFIX}m68k-elf2aout
ELF2ECOFF=	${TOOLDIR}/bin/${_TOOL_PREFIX}mips-elf2ecoff
INSTALL=	${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-install
LEX=		${TOOLDIR}/bin/${_TOOL_PREFIX}lex
LINT=		CC=${CC:Q} ${TOOLDIR}/bin/${MACHINE_GNU_PLATFORM}-lint
LORDER=		NM=${NM:Q} MKTEMP=${TOOL_MKTEMP:Q} ${TOOLDIR}/bin/${_TOOL_PREFIX}lorder
MKDEP=		CC=${CC:Q} ${TOOLDIR}/bin/${_TOOL_PREFIX}mkdep
MKDEPCXX=	CC=${CXX:Q} ${TOOLDIR}/bin/${_TOOL_PREFIX}mkdep
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
TOOL_DISKLABEL=		${TOOLDIR}/bin/nbdisklabel
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
TOOL_MKNOD=		${TOOLDIR}/bin/${_TOOL_PREFIX}mknod
TOOL_MKTEMP=		${TOOLDIR}/bin/${_TOOL_PREFIX}mktemp
TOOL_MKUBOOTIMAGE=	${TOOLDIR}/bin/${_TOOL_PREFIX}mkubootimage
TOOL_ELFTOSB=		${TOOLDIR}/bin/${_TOOL_PREFIX}elftosb
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
TOOL_TOPROTO=		${TOOLDIR}/bin/${_TOOL_PREFIX}toproto
TOOL_UUDECODE=		${TOOLDIR}/bin/${_TOOL_PREFIX}uudecode
TOOL_VGRIND=		${TOOLDIR}/bin/${_TOOL_PREFIX}vgrind -f
TOOL_ZIC=		${TOOLDIR}/bin/${_TOOL_PREFIX}zic

.else	# USETOOLS != yes						# } {

# Clang supports C, C++ and Objective C
TOOL_CC.clang=		clang
TOOL_CPP.clang=		clang-cpp
TOOL_CXX.clang=		clang++
TOOL_OBJC.clang=	clang
TOOL_OPT.clang=		opt
TOOL_LLC.clang=		llc

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
.if defined(__MINIX)
# LSC: A full path has to be provided, as this is also, used as a make
#      target.
TOOL_CLANG_TBLGEN=	/usr/bin/clang-tblgen
.else
TOOL_CLANG_TBLGEN=	clang-tblgen
.endif # defined(__MINIX)
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
.if defined(__MINIX)
# LSC: A full path has to be provided, as this is also, used as a make
#      target.
TOOL_LLVM_TBLGEN=	/usr/bin/llvm-tblgen
.else
TOOL_LLVM_TBLGEN=	llvm-tblgen
.endif # defined(__MINIX)
TOOL_M4=		m4
TOOL_MACPPCFIXCOFF=	macppc-fixcoff
TOOL_MAKEFS=		makefs
TOOL_MAKEINFO=		makeinfo
TOOL_MAKEWHATIS=	/usr/libexec/makewhatis
TOOL_MANDOC_ASCII=	mandoc -Tascii
TOOL_MANDOC_HTML=	mandoc -Thtml
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
TOOL_MKNOD=		mknod
TOOL_MKTEMP=		mktemp
TOOL_MKUBOOTIMAGE=	mkubootimage
TOOL_ELFTOSB=		elftosb
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
.if defined(__MINIX)
# LSC: There is a tic packaged, which has a completly different set of
#      options, so make sure to use the base system one, always.
TOOL_TIC=		/usr/bin/tic
.else
TOOL_TIC=		tic
.endif # defined(__MINIX)
TOOL_TOPROTO=		toproto
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
TOOL_OPT.false=		false
TOOL_LLC.false=		false

AVAILABLE_COMPILER?=	${HAVE_PCC:Dpcc} ${HAVE_LLVM:Dclang} ${HAVE_GCC:Dgcc} false

.for _t in CC CPP CXX FC OBJC OPT LLC
ACTIVE_${_t}=	${AVAILABLE_COMPILER:@.c.@ ${ !defined(UNSUPPORTED_COMPILER.${.c.}) && defined(TOOL_${_t}.${.c.}) :? ${.c.} : }@:[1]}
SUPPORTED_${_t}=${AVAILABLE_COMPILER:Nfalse:@.c.@ ${ !defined(UNSUPPORTED_COMPILER.${.c.}) && defined(TOOL_${_t}.${.c.}) :? ${.c.} : }@}
.endfor
# make bugs prevent moving this into the .for loop
CC=		${TOOL_CC.${ACTIVE_CC}}
CPP=		${TOOL_CPP.${ACTIVE_CPP}}
CXX=		${TOOL_CXX.${ACTIVE_CXX}}
FC=		${TOOL_FC.${ACTIVE_FC}}
OBJC=		${TOOL_OBJC.${ACTIVE_OBJC}}
OPT=		${TOOL_OPT.${ACTIVE_OPT}}
LLC=		${TOOL_LLC.${ACTIVE_LLC}}

.if exists(/usr/bin/${TOOL_CTFCONVERT}) || exists(${TOOL_CTFCONVERT})
CTFCONVERT=	${TOOL_CTFCONVERT}
CTFMERGE=	${TOOL_CTFMERGE}
.endif

# OBJCOPY flags to create a.out binaries for old firmware
# shared among src/distrib and ${MACHINE}/conf/Makefile.${MACHINE}.inc
.if ${MACHINE_CPU} == "arm"
OBJCOPY_ELF2AOUT_FLAGS?=	\
	-O a.out-arm-netbsd	\
	-R .ident		\
	-R .ARM.attributes	\
	-R .ARM.exidx		\
	-R .ARM.extab		\
	-R .arm.atpcs		\
	-R .comment		\
	-R .debug_abbrev	\
	-R .debug_aranges	\
	-R .debug_info		\
	-R .debug_line		\
	-R .debug_frame		\
	-R .debug_loc		\
	-R .debug_pubnames	\
	-R .debug_pubtypes	\
	-R .debug_str		\
	-R .eh_frame		\
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
GNU_ARCH.coldfire=m5407
GNU_ARCH.earm=arm
GNU_ARCH.earmhf=arm
GNU_ARCH.earmeb=armeb
GNU_ARCH.earmhfeb=armeb
GNU_ARCH.earmv4=armv4
GNU_ARCH.earmv4eb=armv4eb
GNU_ARCH.earmv5=arm
GNU_ARCH.earmv5eb=armeb
GNU_ARCH.earmv6=armv6
GNU_ARCH.earmv6hf=armv6
GNU_ARCH.earmv6eb=armv6eb
GNU_ARCH.earmv6hfeb=armv6eb
GNU_ARCH.earmv7=armv7
GNU_ARCH.earmv7hf=armv7
GNU_ARCH.earmv7eb=armv7eb
GNU_ARCH.earmv7hfeb=armv7eb
GNU_ARCH.i386=i486
GCC_CONFIG_ARCH.i386=i486
GCC_CONFIG_TUNE.i386=nocona
GCC_CONFIG_TUNE.x86_64=nocona
GNU_ARCH.m68000=m68010
GNU_ARCH.sh3eb=sh
GNU_ARCH.sh3el=shle
GNU_ARCH.mips64eb=mips64

.if defined(__MINIX)
# MINIX/arm default
GCC_CONFIG_ARCH.earm=armv7-a

# MINIX/intel default
GNU_ARCH.i386=i586
GCC_CONFIG_ARCH.i386=i586

.endif # defined(__MINIX)

MACHINE_GNU_ARCH=${GNU_ARCH.${MACHINE_ARCH}:U${MACHINE_ARCH}}

#
# In order to identify NetBSD to GNU packages, we sometimes need
# an "elf" tag for historically a.out platforms.
#
.if (!empty(MACHINE_ARCH:Mearm*))
MACHINE_GNU_PLATFORM?=${MACHINE_GNU_ARCH}--netbsdelf-${MACHINE_ARCH:C/eb//:C/v[4-7]//:S/earm/eabi/}
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

# We need to check for HAVE_GOLD after LD has been set
.if ${_HAVE_GOLD:U} == ""
   _HAVE_GOLD!= (exec 2>&1; ${LD} --version || echo "")
   _GOLD_MATCH:=${_HAVE_GOLD:Mgold}
   _HAVE_GOLD:= ${_HAVE_GOLD:M[0-9]\.[0-9][0-9]}
.  if ${_GOLD_MATCH} != "" && ${_HAVE_GOLD} != ""
      HAVE_GOLD?= ${_HAVE_GOLD}
#     CFLAGS+= -DHAVE_GOLD=${_HAVE_GOLD}
#     AFLAGS+= -DHAVE_GOLD=${_HAVE_GOLD}
.  else
      USE_BITCODE:=no
.  endif
.endif # ${_HAVE_GOLD:U} == ""
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
.elif !empty(MACHINE_ARCH:Mearm*)
MKCOMPAT?=	no
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
# arm is always softfloat unless it isn't
# emips is always softfloat.
# coldfire is always softfloat
#
.if ${MACHINE_ARCH} == "mips64eb" || ${MACHINE_ARCH} == "mips64el" || \
    (${MACHINE_CPU} == "arm" && ${MACHINE_ARCH:M*hf*} == "") || \
    ${MACHINE_ARCH} == "coldfire" || \
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
	MKLDAP MKLIBSTDCXX MKLINKLIB MKLVM \
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
	MKSYSDEBUG MKLIVEUPDATE MKLWIP
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
	MKKYUA MKLLD MKLLDB MKLINT \
	MKMANZ MKMCLINKER MKOBJDIRS \
	MKLIBCXX MKLLVM MKPCC \
	MKPIGZGZIP \
	MKREPRO \
	MKSOFTFLOAT MKSTRIPIDENT MKTPM \
	MKUNPRIVED MKUPDATE MKX11 MKX11MOTIF MKZFS

#MINIX-specific vars
_MKVARS.no+= \
	MKIMAGEONLY MKSMALL MKBITCODE MKSRC
.if !empty(MACHINE_ARCH:Mearm*)
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
# Which platforms build the xorg-server drivers (as opposed
# to just Xnest and Xvfb.)
#
.if ${X11FLAVOUR} == "Xorg"	&& \
    ${MACHINE} == "alpha"	|| \
    ${MACHINE} == "amd64"	|| \
    ${MACHINE} == "bebox"	|| \
    ${MACHINE} == "cats"	|| \
    ${MACHINE} == "dreamcast"	|| \
    ${MACHINE} == "ews4800mips"	|| \
    ${MACHINE} == "evbarm"	|| \
    ${MACHINE} == "evbmips"	|| \
    ${MACHINE} == "hp300"	|| \
    ${MACHINE} == "hpcarm"	|| \
    ${MACHINE} == "hpcmips"	|| \
    ${MACHINE} == "hpcsh"	|| \
    ${MACHINE} == "i386"	|| \
    ${MACHINE} == "luna68k"	|| \
    ${MACHINE} == "macppc"	|| \
    ${MACHINE} == "netwinder"	|| \
    ${MACHINE} == "newsmips"	|| \
    ${MACHINE} == "prep"	|| \
    ${MACHINE} == "ofppc"	|| \
    ${MACHINE} == "sgimips"	|| \
    ${MACHINE} == "shark"	|| \
    ${MACHINE} == "sparc"	|| \
    ${MACHINE} == "sparc64"	|| \
    ${MACHINE} == "vax"		|| \
    ${MACHINE} == "zaurus"
MKXORG_SERVER?=yes
.else
MKXORG_SERVER?=no
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
MKDEBUGREG:=	no
MKSYSDEBUG:=	no
MKLIVEUPDATE:=	no
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
	USE_WATCHDOG USE_ACPI USE_APIC USE_DEBUGREG USE_SYSDEBUG \
	USE_LIVEUPDATE USE_PCI USE_BITCODE
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
	xkeyboard-config xinput xcb-util xorg-docs \
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
