#	$NetBSD: Makefile,v 1.189 2012/09/16 13:47:41 rmind Exp $
#	from: @(#)Makefile	5.25.1.1 (Berkeley) 5/7/91

.include <bsd.own.mk>

SUBDIR=		csu .WAIT

.if (${MKGCC} != "no")
. if ${HAVE_GCC} == 4
.  if (${USE_COMPILERCRTSTUFF} == "yes")
SUBDIR+=	../gnu/lib/crtstuff4 .WAIT
.  endif
SUBDIR+=	../gnu/lib/libgcc4 .WAIT
. else
.  if (${USE_COMPILERCRTSTUFF} == "yes")
SUBDIR+=	../external/gpl3/gcc/lib/crtstuff .WAIT
.  endif
SUBDIR+=	../external/gpl3/gcc/lib/libgcc .WAIT
. endif
.endif

#LSC MINIX libc depends on libminlib because of minix/malloc-debug.c
SUBDIR+=	libminlib
SUBDIR+=	.WAIT

SUBDIR+=	libsys
SUBDIR+=	.WAIT

SUBDIR+=	libc
SUBDIR+=	.WAIT

#
# The SUBDIRs above are included here for completeness but should be built
# and installed prior to make(dependall) in this file, as libraries listed
# below will depend on versions from DESTDIR only.
#

#SUBDIR+=	i18n_module

SUBDIR+= libcompat_minix libblockdriver libchardriver     \
       libnetdriver libm libtimers libutil \
       libz libfetch libvtreefs libaudiodriver libmthread   \
       libexec libdevman libusb libasyn librmt \
       libddekit libminixfs libbdev libelf libminc libcrypt libterminfo \
       libutil libbz2 libprop \
       libpuffs libsffs

.if (${MKLWIP} == "yes")
SUBDIR += liblwip libnetsock
.endif

.if (${MACHINE_ARCH} == "i386")
SUBDIR += libvassert libhgfs libvboxfs libvirtio
.endif

.if (${MACHINE_ARCH} == "earm")
SUBDIR += libclkconf libgpio libi2cdriver libpadconf
.endif

.if (${MKRUMP} != "no")
SUBDIR+=	librumpclient
.endif
.if (${MKSKEY} != "no")
SUBDIR+=	libskey
.endif

.if (${MKCRYPTO} != "no")
SUBDIR+=	../crypto/external/bsd/netpgp/libmj
.endif

.if (${MKMDNS} != "no")
SUBDIR+=	../external/apache2/mDNSResponder/lib
.endif

#SUBDIR+=	../external/bsd/am-utils/lib

SUBDIR+=	../external/bsd/flex/lib
#SUBDIR+=	../external/bsd/tre/lib
#SUBDIR+=	../external/bsd/libdwarf/lib
#SUBDIR+=	../external/bsd/libelf/lib
#SUBDIR+=	../external/bsd/libevent/lib
#SUBDIR+=	../external/bsd/liblzf/lib
#SUBDIR+=	../external/bsd/libpcap/lib

SUBDIR+=	../external/mit/lua/lib
#SUBDIR+=	../external/mit/expat/lib

SUBDIR+=	../external/public-domain/sqlite/lib
SUBDIR+=	../external/public-domain/xz/lib

#SUBDIR+=	../gnu/lib/libmalloc

.if (${MKGCC} != "no")
. if ${HAVE_GCC} == 4
#SUBDIR+=	../gnu/lib/libobjc4
. else
#SUBDIR+=	../external/gpl3/gcc/lib/libobjc
#SUBDIR+=	../external/gpl3/gcc/lib/libgomp
. endif
# LSC: Replaced __MINIX test with MKGCCCMDS, as we do not want gcc by default
#      nor the libraries, but we still want toretain the possibility of 
#      installing GCC from the base system.
. if ${HAVE_GCC} >= 45 && !defined(MLIBDIR) && ${MKGCCCMDS} == "yes"
# Should probably move GMP, MPFR and MPC builds into the GCC >= 4.5
# specific build area, but we get better parallelism this way.
SUBDIR+=	../external/lgpl3/gmp/lib/libgmp
SUBDIR+=	../external/lgpl3/mpfr/lib/libmpfr
SUBDIR+=	../external/lgpl2/mpc/lib/libmpc
. endif
.endif

#
# Libraries that depend upon any listed previously
# (and those that depend upon these [and ...])
#
#==================== 1st library dependency barrier ====================
SUBDIR+=	.WAIT

SUBDIR+=	libcurses	# depends on libterminfo
#SUBDIR+=	libdm		# depends on libprop
SUBDIR+=	libedit		# depends on libterminfo
#SUBDIR+=	libexecinfo 	# depends on libelf
#SUBDIR+=	libppath	# depends on libprop
#SUBDIR+=	libperfuse	# depends on libpuffs
#SUBDIR+=	libquota	# depends on libprop and librpcsvc
SUBDIR+=	librefuse	# depends on libpuffs
.if (${MKRUMP} != "no")
SUBDIR+=	librumpuser	# depends on libpthread
SUBDIR+=	librumphijack	# depends on librumpclient and libpthread
.endif

.if (${MKNPF} != "no")
SUBDIR+=	libnpf		# depends on libprop
#SUBDIR+=	npf
.endif

.if (${MKCRYPTO} != "no")
SUBDIR+=	../crypto/external/bsd/openssl/lib # depends on libcrypt
.endif

SUBDIR+=	../external/bsd/file/lib	# depends on libz

.if (${MKISCSI} != "no")
SUBDIR+=	../external/bsd/iscsi/lib	# depends on libpthread
.endif

SUBDIR+=	../external/bsd/libarchive/lib	# depends on libxz

.if (${MKLVM} != "no")
SUBDIR+=	../external/gpl2/lvm2/lib	# depends on libprop
.endif

.if (${MKBINUTILS} != "no")
SUBDIR+=	../external/gpl3/binutils/lib	# libbfd depends on libz
.endif

.if (${MKGCC} != "no" && ${MKCXX} != "no")
.if (defined(__MINIX) && ${MKGCCCMDS} == "yes")
. if ${HAVE_GCC} == 4
SUBDIR+=	../gnu/lib/libstdc++-v3_4	# depends on libm
SUBDIR+=	../gnu/lib/libsupc++4
. else
SUBDIR+=	../external/gpl3/gcc/lib/libstdc++-v3
SUBDIR+=	../external/gpl3/gcc/lib/libsupc++
. endif
.endif # (defined(__MINIX) && ${MKGCCCMDS} == "yes")
.endif

#==================== 2nd library dependency barrier ====================
SUBDIR+=	.WAIT

.if (defined(__MINIX) && ${MKGCCCMDS} == "yes")
.if (${MKATF} != "no")
SUBDIR+=	../external/bsd/atf/lib		# depends on libstdc++
.endif

.if (${MKKYUA} != "no")
SUBDIR+=	../external/bsd/lutok/lib	# depends on lua and libstdc++
.endif
.endif # (defined(__MINIX) && ${MKGCCCMDS} == "yes")

#SUBDIR+=	libform		# depends on libcurses
#SUBDIR+=	libmenu		# depends on libcurses
#SUBDIR+=	libradius	# depends on libcrypto if (${MKCRYPTO} != "no")
.if (${MKRUMP} != "no")
SUBDIR+=	librump		# depends on librumpuser
.endif

.if (${MKKERBEROS} != "no")
SUBDIR+=	../crypto/external/bsd/heimdal/lib	# depends on libcrypto
							# libedit, libterminfo,
.endif

.if (${MKCRYPTO} != "no")
SUBDIR+=	../crypto/external/bsd/openssh/lib # depends on libcrypto, libz
SUBDIR+=	../crypto/external/bsd/netpgp/lib  # depends on libcrypto, ...
.endif

#SUBDIR+=	../external/bsd/bind/lib	# depends on libcrypto
#SUBDIR+=	../external/bsd/fetch/lib	# depends on libssl

.if (${MKLDAP} != "no")
SUBDIR+=	../external/bsd/openldap/lib	# depends on libcrypto, ...
.endif

#==================== 3rd library dependency barrier ====================
SUBDIR+=	.WAIT

.if (${MKRUMP} != "no")
SUBDIR+=	librumpdev	# depends on librump
SUBDIR+=	librumpnet	# depends on librump
SUBDIR+=	librumpvfs	# depends on librump
.endif

.if (${MKPAM} != "no")
SUBDIR+=	libpam		# depends on heimdal
.endif

.if (${MKCRYPTO} != "no")
SUBDIR+=	../crypto/external/bsd/libsaslc	# depends on heimdal, openssl
.endif

#SUBDIR+=	../external/bsd/mdocml/lib

.if (${MKRUMP} != "no")
#==================== 4th library dependency barrier ====================
SUBDIR+=	.WAIT

SUBDIR+=	libukfs		# depends on librumpvfs, librump

#==================== 5th library dependency barrier ====================
SUBDIR+=	.WAIT

SUBDIR+=	libp2k		# depends on libukfs, librumpvfs, libpuffs

.if !defined(BSD_MK_COMPAT_FILE)
SUBDIR+=	../sys/rump/dev/lib
SUBDIR+=	../sys/rump/fs/lib
SUBDIR+=	../sys/rump/kern/lib
SUBDIR+=	../sys/rump/net/lib
.endif
.endif

# Lua bindings come last, they might depend on anything
#SUBDIR+=	lua

#
# build_install logic for src/Makefile.
# Compute a list of subdirectories delimited by .WAIT.
# Run "make dependall && make install" for all subdirectories in a group
# concurrently, but wait after each group.
#
SUBDIR_GROUPS=	1
CUR_GROUP:=	1
.for dir in ${SUBDIR}
.  if ${dir} == ".WAIT"
CUR_GROUP:=	${CUR_GROUP}1
SUBDIR_GROUPS:=	${SUBDIR_GROUPS} ${CUR_GROUP}
.  else
SUBDIR_GROUP.${CUR_GROUP}+=	${dir}
.endif

.endfor

build_install:
.for group in ${SUBDIR_GROUPS}
.  if !empty(SUBDIR_GROUP.${group})
	${MAKEDIRTARGET} . ${SUBDIR_GROUP.${group}:C/^/dependall-/}
	${MAKEDIRTARGET} . ${SUBDIR_GROUP.${group}:C/^/install-/}
.  endif
.endfor

.include <bsd.subdir.mk>
