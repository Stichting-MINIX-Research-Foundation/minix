CHFS=	${NETBSDSRCDIR}/sys/ufs/chfs

.PATH:	${.CURDIR}/chfs ${NETBSDSRCDIR}/sys/ufs/chfs

CPPFLAGS+= -I${CHFS}

SRCS+=	chfs_mkfs.c
.if !defined(HOSTPROG)
LDADD+= -lz
DPADD+= ${LIBZ}
.endif
