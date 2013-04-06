#	$NetBSD: Makefile,v 1.78 2012/08/10 16:05:27 joerg Exp $

LIB=	sa
NOPIC=	# defined
NOPROFILE=# defined

SA_USE_CREAD?= no		# Read compressed kernels
SA_INCLUDE_NET?= yes		# Netboot via TFTP, NFS
SA_USE_LOADFILE?= no		# Generic executable loading support
SA_ENABLE_LS_OP?= no		# Filesystems ls operation

#DEBUGCPPFLAGS= -DBOOTP_DEBUG -DNETIF_DEBUG -DETHER_DEBUG -DNFS_DEBUG -DRPC_DEBUG -DRARP_DEBUG -DARP_DEBUG -DNET_DEBUG -DDEBUG -DPARANOID
CPPFLAGS=	-I${SADIR} ${SACPPFLAGS} ${SAMISCCPPFLAGS} \
		-DCOMPAT_UFS ${DEBUGCPPFLAGS}

#COPTS+= -ansi -pedantic -Wall

.if defined(SA_EXTRADIR)
.-include "${SA_EXTRADIR}/Makefile.inc"
.endif

.include <bsd.own.mk>

.PATH.c: ${SADIR} ${.PARSEDIR}/../../../common/lib/libc/string

# stand routines
SRCS+=	alloc.c errno.c exit.c files.c \
	getfile.c gets.c globals.c \
	panic.c printf.c qsort.c snprintf.c sprintf.c strerror.c \
	subr_prf.c twiddle.c vsprintf.c checkpasswd.c

.if (${MACHINE_CPU} != "mips") && !defined(__MINIX)
SRCS+=	exec.c
.endif

# string routines
.if ${MACHINE_ARCH} != "i386" && ${MACHINE_ARCH} != "x86_64"
SRCS+=	memcmp.c memcpy.c memmove.c memset.c strchr.c
.endif

.ifndef __MINIX
SRCS+=	bcopy.c bzero.c	# Remove me eventually.
.endif

# io routines
SRCS+=	closeall.c dev.c disklabel.c dkcksum.c ioctl.c nullfs.c stat.c fstat.c
SRCS+=	close.c lseek.c open.c read.c write.c
.if (${SA_USE_CREAD} == "yes")
CPPFLAGS+= -D__INTERNAL_LIBSA_CREAD
SRCS+=	cread.c
.endif
.if (${SA_ENABLE_LS_OP} == "yes")
SRCS+= ls.c
.endif

.if (${SA_USE_LOADFILE} == "yes")
.ifndef __MINIX
SRCS+=	loadfile.c loadfile_ecoff.c loadfile_elf32.c lookup_elf32.c \
	loadfile_elf64.c lookup_elf64.c
.if (${MACHINE_CPU} != "mips")
SRCS+=	loadfile_aout.c
.endif
.else
SRCS+=	loadfile.c loadfile_elf32.c loadfile_elf64.c
.endif
.endif

.if (${SA_INCLUDE_NET} == "yes")
# network routines
SRCS+=	arp.c ether.c ether_sprintf.c ip_cksum.c net.c netif.c rpc.c udp.c ip.c

# network info services:
SRCS+=	bootp.c rarp.c bootparam.c

# boot filesystems
SRCS+=	nfs.c tftp.c
.endif

SRCS+=	ffsv1.c ffsv2.c
SRCS+=	lfsv1.c lfsv2.c
SRCS+=	cd9660.c
SRCS+=	ustarfs.c
SRCS+=	dosfs.c
SRCS+=	ext2fs.c
SRCS+=	minixfs3.c
SRCS+=	fnmatch.c
# for historic compatibility ufs == ffsv1
SRCS+=	ufs.c

.if defined(__MINIX)
# NetBSD has it in libkern, MINIX lacks it
SRCS+=	xlat_mbr_fstype.c

# NetBSD has it in libkern, MINIX has it in libc but not libminc...
SRCS+=	md5c.c
.PATH.c: ${.PARSEDIR}/../../../common/lib/libc/md
.endif

# only needed during build
libinstall::

.undef DESTDIR
.include <bsd.lib.mk>

lib${LIB}.o:: ${OBJS}
	@echo building standard ${LIB} library
	@rm -f lib${LIB}.o
	@${LD} -r -o lib${LIB}.o `lorder ${OBJS} | tsort`

CPPFLAGS+=	-Wno-pointer-sign

.if defined(HAVE_GCC) && ${HAVE_GCC} >= 45 && ${MACHINE_ARCH} == "vax"
COPTS.bootp.c+=	-O0
.endif
