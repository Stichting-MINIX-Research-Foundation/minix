#	$NetBSD: Makefile,v 1.50 2014/08/10 23:39:08 matt Exp $
#	from: @(#)Makefile	8.1 (Berkeley) 6/4/93

WARNS=	3

USE_FORT?= yes	# used primarily by setgid programs

USE_SHLIBDIR=	yes

LIB=	kvm
CPPFLAGS+=-DLIBC_SCCS -I${NETBSDSRCDIR}/sys -D_KMEMUSER

SRCS=	kvm.c kvm_file.c kvm_getloadavg.c kvm_proc.c

# This library should compile to the same thing on all variants of
# ${MACHINE} with the same ${MACHINE_ARCH} so shared /usr works.
# If it is necessary to run different code based on ${MACHINE} then
# the dispatch on ${MACHINE} should be done at run time (see m68k).

.include <bsd.own.mk>
.if exists(kvm_${KVM_MACHINE_ARCH}.c)
SRCS+=	kvm_${KVM_MACHINE_ARCH}.c
.elif exists(kvm_${KVM_MACHINE_CPU}.c)
SRCS+=	kvm_${KVM_MACHINE_CPU}.c
.elif exists(kvm_${MACHINE_ARCH}.c)
SRCS+=	kvm_${MACHINE_ARCH}.c
.elif  exists(kvm_${MACHINE_CPU}.c)
SRCS+=	kvm_${MACHINE_CPU}.c
.else
.BEGIN:
	@echo no kvm_xx.c for ${MACHINE_ARCH} nor ${MACHINE_CPU}
	@false
.endif

.if ${MACHINE_ARCH} == "i386"
LINTFLAGS+=-w
SRCS+=	kvm_i386pae.c # Hook PAE support in the i386 build
.endif

# Additional modules needed for m68k
.if (${MACHINE_ARCH} == "m68k" || ${MACHINE_CPU} == "m68k")
SRCS+=	kvm_m68k_cmn.c kvm_sun2.c kvm_sun3.c kvm_sun3x.c
.endif

MAN=	kvm.3 kvm_dump.3 kvm_geterr.3 kvm_getfiles.3 kvm_getloadavg.3 \
	kvm_getkernelname.3 \
	kvm_getprocs.3 kvm_nlist.3 kvm_open.3 kvm_read.3 kvm_getlwps.3

MLINKS+=kvm_getprocs.3 kvm_getargv.3 kvm_getprocs.3 kvm_getenvv.3
MLINKS+=kvm_getprocs.3 kvm_getproc2.3
MLINKS+=kvm_getprocs.3 kvm_getargv2.3 kvm_getprocs.3 kvm_getenvv2.3
MLINKS+=kvm_open.3 kvm_openfiles.3 kvm_open.3 kvm_close.3
MLINKS+=kvm_read.3 kvm_write.3
MLINKS+=kvm_dump.3 kvm_dump_mkheader.3 kvm_dump.3 kvm_dump_wrtheader.3
MLINKS+=kvm_dump.3 kvm_dump_inval.3

# This fixes building with MACHINE==sparc64, MACHINE_ARCH==sparc
# XXX:	this is a hack, but until the sparc MD headers are separated
#	out from the sparc AD headers, it's the easiest solution.
#
.if (${MACHINE_ARCH} == "sparc")	# {
.if !make(obj) && !make(clean) && !make(cleandir)
.BEGIN:
	@([ -h machine ] || \
	    ln -fs ${NETBSDSRCDIR}/sys/arch/sparc/include machine)
	@([ -h sparc ] || ln -fs ${NETBSDSRCDIR}/sys/arch/sparc/include sparc)
.NOPATH: machine sparc
.endif
CLEANFILES+=	machine sparc
CPPFLAGS+=	-I.
.endif					# }


.include <bsd.lib.mk>
