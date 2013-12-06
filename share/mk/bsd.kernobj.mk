#	$NetBSD: bsd.kernobj.mk,v 1.14 2013/06/03 07:39:07 mrg Exp $

# KERNSRCDIR	Is the location of the top of the kernel src.
# 		It defaults to `${NETBSDSRCDIR}/sys'.
#
# KERNARCHDIR	Is the location of the machine dependent kernel sources.
#		It defaults to `arch/${MACHINE}', but may be overridden
#		in case ${MACHINE} is not correct.
#
# KERNCONFDIRDEFAULT	Is the default for ${KERNCONFDIR}.
#		It defaults to `${KERNSRCDIR}/${KERNARCHDIR}/conf'.
#
# KERNCONFDIR	Is where the configuration files for kernels are found.
#		Users can set this to have build.sh find kernel
#		configurations in another directory.
#		It defaults to `${KERNCONFDIRDEFAULT}'.
#
# KERNOBJDIR	Is the kernel build directory.  The kernel GENERIC for
# 		instance will be compiled in ${KERNOBJDIR}/GENERIC.
# 		The default is the .OBJDIR of
#		`${KERNSRCDIR}/${KERNARCHDIR}/compile'.
#

.include <bsd.own.mk>

KERNSRCDIR?=		${NETBSDSRCDIR}/sys
KERNARCHDIR?=		arch/${MACHINE}
KERNCONFDIRDEFAULT?=	${KERNSRCDIR}/${KERNARCHDIR}/conf
KERNCONFDIR?=		${KERNCONFDIRDEFAULT}

.if !defined(KERNOBJDIR) && exists(${KERNSRCDIR}/${KERNARCHDIR}/compile)
KERNOBJDIR!=	cd "${KERNSRCDIR}/${KERNARCHDIR}/compile" && ${PRINTOBJDIR}
.endif
