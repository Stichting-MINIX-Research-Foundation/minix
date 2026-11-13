#	$NetBSD: Makefile.inc,v 1.4 2020/09/06 07:20:31 mrg Exp $
#

UDF=	${NETBSDSRCDIR}/sys/fs/udf
UDF_NEWFS=	${NETBSDSRCDIR}/sbin/newfs_udf
FSCK=	${NETBSDSRCDIR}/sbin/fsck	# use progress meter.

.PATH:	${.CURDIR}/udf ${UDF} ${UDF_NEWFS} ${FSCK}

CPPFLAGS+= -I${UDF} -I${UDF_NEWFS} -I${FSCK}

SRCS += udf_create.c udf_write.c udf_osta.c

.if !defined(__MINIX)
CWARNFLAGS.clang+=	-Wno-error=address-of-packed-member -Wno-error=constant-conversion
.endif # !defined(__MINIX)
CWARNFLAGS.gcc+=	${GCC_NO_ADDR_OF_PACKED_MEMBER}
