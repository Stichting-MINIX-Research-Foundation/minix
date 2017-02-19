#	$NetBSD: Makefile,v 1.235 2015/09/28 17:12:12 christos Exp $
#	from: @(#)Makefile	5.25.1.1 (Berkeley) 5/7/91

.include <bsd.own.mk>

SUBDIR=		csu .WAIT

.if (${MKGCC} != "no")
SUBDIR+=	../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libgcc .WAIT
.endif

.if defined(__MINIX)
#LSC MINIX libc depends on
# - libmthread because of sys/lib/libunwind
SUBDIR+=	.WAIT

SUBDIR+=	../minix/lib/libsys
SUBDIR+=	../minix/lib/libmthread
SUBDIR+=	.WAIT
.endif # defined(__MINIX)
SUBDIR+=	libc
SUBDIR+=	.WAIT

.if defined(__MINIX)
SUBDIR+=	../minix/lib/libasyn \
		../minix/lib/libaudiodriver \
		../minix/lib/libbdev \
		../minix/lib/libblockdriver \
		../minix/lib/libchardriver \
		../minix/lib/libddekit \
		../minix/lib/libdevman \
		../minix/lib/libexec \
		../minix/lib/libfsdriver \
		../minix/lib/libinputdriver \
		../minix/lib/liblwip \
		../minix/lib/libminc \
		../minix/lib/libminixfs \
		../minix/lib/libnetdriver \
		../minix/lib/libsffs \
		../minix/lib/libsockdriver \
		../minix/lib/libsockevent \
		../minix/lib/libtimers \
		../minix/lib/libusb \
		../minix/lib/libvtreefs

.if (${HAVE_LIBGCC} == "no")
SUBDIR+=	../minix/lib/libgcc_s_empty
.endif

.if (${MACHINE_ARCH} == "i386")
SUBDIR+=	../minix/lib/libacpi \
		../minix/lib/libhgfs \
		../minix/lib/libvassert \
		../minix/lib/libvboxfs \
		../minix/lib/libvirtio
.endif

.if (${MACHINE_ARCH} == "earm")
SUBDIR+=	../minix/lib/libclkconf \
		../minix/lib/libgpio \
		../minix/lib/libi2cdriver
.endif

.if (${MKMAGIC} == "yes")
SUBDIR+=	../minix/lib/libmagicrt
.endif

.endif # defined(__MINIX)
#
# The SUBDIRs above are included here for completeness but should be built
# and installed prior to make(dependall) in this file, as libraries listed
# below will depend on versions from DESTDIR only.
#

SUBDIR+=	i18n_module

SUBDIR+=	\
		libbz2 \
		libcrypt \
		libintl libkvm libm \
		libpci libprop \
		libpuffs librmt \
		libtelnet libterminfo \
		libutil libwrap libz

.if !defined(BSD_MK_COMPAT_FILE)
#SUBDIR+=	libkern
.endif

.if (${MACHINE_CPU} == "arm" || ${MACHINE_CPU} == "aarch64")
LIBC_MACHINE_ARCH?=${MACHINE_ARCH}
LIBC_MACHINE_CPU?=${MACHINE_CPU}
.if ${LIBC_MACHINE_CPU} == "arm" && empty(LIBC_MACHINE_ARCH:M*hf*)
.if !defined(__MINIX)
SUBDIR+=	libc_vfp
.endif # !defined(__MINIX)
.endif
.endif
.if (${MKRUMP} != "no")
SUBDIR+=	librumpclient
.endif
.if (${MKSKEY} != "no")
SUBDIR+=	libskey
.endif

.if (${MKMDNS} != "no")
SUBDIR+=	../external/apache2/mDNSResponder/lib
.endif

#SUBDIR+=	../external/bsd/am-utils/lib
SUBDIR+=	../external/bsd/blacklist/lib

SUBDIR+=	../external/bsd/flex/lib
#SUBDIR+=	../external/bsd/tre/lib
SUBDIR+=	../external/bsd/elftoolchain/lib/libelf
#SUBDIR+=	../external/bsd/liblzf/lib
SUBDIR+=	../external/bsd/libpcap/lib

.if ${MKSLJIT} != "no"
SUBDIR+=	../external/bsd/sljit/lib
SUBDIR+=	libbpfjit
.endif

.if (${MKZFS} != "no")
SUBDIR+=        ../external/cddl/osnet/lib/libavl
SUBDIR+=        ../external/cddl/osnet/lib/libnvpair
SUBDIR+=        ../external/cddl/osnet/lib/libumem
SUBDIR+=        ../external/cddl/osnet/lib/libuutil
.endif

SUBDIR+=	../external/mit/expat/lib

SUBDIR+=	../external/public-domain/sqlite/lib

#SUBDIR+=	../gnu/lib/libmalloc

.if (${MKGCC} != "no")
. if !defined(__MINIX) || ${MKLIBSTDCXX} == "yes"
SUBDIR+=	../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libobjc
SUBDIR+=	../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libgomp
#SUBDIR+=	../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libmudflap
#SUBDIR+=	../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libmudflapth
. endif # !defined(__MINIX) || ${MKLIBSTDCXX} == "yes"
. if !defined(__MINIX) || ${MKLIBSTDCXX} == "yes"
# Should probably move GMP, MPFR and MPC builds into the GCC >= 4.5
# specific build area, but we get better parallelism this way.
# We don't build compat versions of these.
. if !defined(MLIBDIR)
SUBDIR+=	../external/lgpl3/gmp/lib/libgmp
SUBDIR+=	../external/lgpl3/mpfr/lib/libmpfr
SUBDIR+=	../external/lgpl3/mpc/lib/libmpc
.   endif # !defined(__MINIX) || ${MKLIBSTDCXX} == "yes"
. endif
.endif

#
# Libraries that depend upon any listed previously
# (and those that depend upon these [and ...])
#
#==================== 1st library dependency barrier ====================
SUBDIR+=	.WAIT

.if ${MKDTRACE} != "no"
SUBDIR+=	../external/bsd/librtld_db/lib		# depends on libutil
.endif

.if ${MKDTRACE} != "no" || ${MKCTF} != "no"
SUBDIR+=        ../external/cddl/osnet/lib/libctf
.endif

SUBDIR+=	../external/public-domain/xz/lib	# depends on libpthread

.if (${MKCRYPTO} != "no")
SUBDIR+=	../crypto/external/bsd/netpgp/libmj
SUBDIR+=	../crypto/external/bsd/netpgp/lib/verify # depends on libz
.endif

#SUBDIR+=	../external/bsd/libdwarf/lib # depends on libelf
SUBDIR+=	../external/mit/lua/lib # depends on libm
SUBDIR+=	libcurses	# depends on libterminfo
#SUBDIR+=	libdm		# depends on libprop
SUBDIR+=	libedit		# depends on libterminfo
SUBDIR+=	libexecinfo 	# depends on libelf
#SUBDIR+=	libppath	# depends on libprop
#SUBDIR+=	libperfuse	# depends on libpuffs
#SUBDIR+=	libquota	# depends on libprop and librpcsvc
SUBDIR+=	librefuse	# depends on libpuffs
#SUBDIR+=	libisns 	# depends on libpthread
.if (${MKRUMP} != "no")
SUBDIR+=	librumpuser	# depends on libpthread
SUBDIR+=	librumphijack	# depends on librumpclient and libpthread
.endif

.if (${MKNPF} != "no")
SUBDIR+=	libnpf		# depends on libprop
.endif

.if (${MKCRYPTO} != "no")
SUBDIR+=	../crypto/external/bsd/openssl/lib # depends on libcrypt
.endif

SUBDIR+=	../external/bsd/file/lib	# depends on libz

.if (${MKISCSI} != "no")
SUBDIR+=	../external/bsd/iscsi/lib	# depends on libpthread
.endif

.if (${MKZFS} != "no")
SUBDIR+=        ../external/cddl/osnet/lib/libzfs
SUBDIR+=        ../external/cddl/osnet/lib/libzpool
.endif

.if (${MKLVM} != "no")
SUBDIR+=	../external/gpl2/lvm2/lib	# depends on libprop
.endif

.if (${MKBINUTILS} != "no")
SUBDIR+=	../external/gpl3/binutils/lib	# libbfd depends on libz
.endif

.if (${MKLIBCXX} != "no")
SUBDIR+=	../external/bsd/libc++
.endif

.if (${MKGCC} != "no" && ${MKCXX} != "no" && ${MKLIBSTDCXX} != "no")
SUBDIR+=	../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libstdc++-v3
SUBDIR+=	../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libsupc++
.endif

#==================== 2nd library dependency barrier ====================
SUBDIR+=	.WAIT

SANITIZER_DIR=../external/gpl3/${EXTERNAL_GCC_SUBDIR}/lib/libasan
.if exists(${SANITIZER_DIR})
#SUBDIR+=	${SANITIZER_DIR}
.endif

SUBDIR+=	../external/bsd/libarchive/lib	# depends on libxz

.if (${MKNPF} != "no")
SUBDIR+=	npf		# depends on libnpf
.endif

.if (${MKATF} != "no")
SUBDIR+=	../external/bsd/atf/lib		# depends on libstdc++
.endif

.if (${MKKYUA} != "no")
SUBDIR+=	../external/bsd/lutok/lib	# depends on lua and libstdc++
.endif

SUBDIR+=	libform		# depends on libcurses
SUBDIR+=	libmenu		# depends on libcurses
#SUBDIR+=	libradius	# depends on libcrypto if (${MKCRYPTO} != "no")
.if (${MKRUMP} != "no")
SUBDIR+=	librump		# depends on librumpuser
.endif

.if (${MKKERBEROS} != "no")
SUBDIR+=	../crypto/external/bsd/heimdal/lib	# depends on libcrypto
							# libedit, libterminfo,
.endif

.if (${MKCRYPTO} != "no")
#SUBDIR+=	../crypto/external/bsd/openssh/lib # depends on libcrypto, libz
SUBDIR+=	../crypto/external/bsd/netpgp/lib  # depends on libcrypto, ...
.endif

SUBDIR+=	../external/bsd/libevent/lib	# depends on libcrypto
.if ${MKDTRACE} != "no"
SUBDIR+=	../external/bsd/libproc/lib	# depends on libstdc++, libctf
.endif
SUBDIR+=	../external/bsd/fetch/lib	# depends on libssl

.if (${MKLDAP} != "no")
SUBDIR+=	../external/bsd/openldap/lib	# depends on libcrypto, ...
.endif

#==================== 3rd library dependency barrier ====================
SUBDIR+=	.WAIT

SUBDIR+=	../external/bsd/bind/lib	# depends on heimdal, libcrypto

.if ${MKDTRACE} != "no"
SUBDIR+=        ../external/cddl/osnet/lib/libdtrace	# depends on libproc
.endif

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

SUBDIR+=	../external/bsd/mdocml/lib

.if (${MKRUMP} != "no")
#==================== 4th library dependency barrier ====================
SUBDIR+=	.WAIT

SUBDIR+=	libukfs		# depends on librumpvfs, librump

.if (${MKTPM} != "no")
SUBDIR+=	../crypto/external/cpl/trousers/lib
.endif

#==================== 5th library dependency barrier ====================
SUBDIR+=	.WAIT

SUBDIR+=	libp2k		# depends on libukfs, librumpvfs, libpuffs

.if (${MKTPM} != "no")
SUBDIR+=	../crypto/external/cpl/tpm-tools/lib	# depends on trousers
.endif

.if !defined(BSD_MK_COMPAT_FILE)
SUBDIR+=	../sys/rump/dev/lib
SUBDIR+=	../sys/rump/fs/lib
SUBDIR+=	../sys/rump/kern/lib
SUBDIR+=	../sys/rump/net/lib
.endif
.endif

# Lua bindings come last, they might depend on anything
SUBDIR+=	lua

# Needed by rump and rescue which are outside the smbfs tree
#SUBDIR+=	../external/bsd/smbfs/lib

.include <bsd.buildinstall.mk>
.include <bsd.subdir.mk>
