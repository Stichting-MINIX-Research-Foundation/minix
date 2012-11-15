#	$NetBSD: Makefile,v 1.12 2011/05/20 22:29:55 joerg Exp $

S=		${.CURDIR}/../../../..

AFLAGS.cdboot.S= ${${ACTIVE_CC} == "clang":?-no-integrated-as:}

NOMAN=		# defined
.include <bsd.own.mk>

.if defined(__MINIX)
AFLAGS+=       -Wa,--divide
.endif

STRIPFLAG=	# nothing

LIBCRT0=	# nothing
LIBCRTBEGIN=	# nothing
LIBCRTEND=	# nothing
LIBC=		# nothing

PRIMARY_LOAD_ADDRESS=	0x1000
SECONDARY_LOAD_ADDRESS=	0x10000

PROG=		bootxx_cd9660
SRCS?=		cdboot.S

BINDIR=		/usr/mdec
BINMODE=	444

.PATH:		${.CURDIR}/..

LDFLAGS+=	-nostdlib -Wl,-e,start
CPPFLAGS+=	-I. -I${.CURDIR}/../lib -I${S}
CPPFLAGS+=	-DPRIMARY_LOAD_ADDRESS=${PRIMARY_LOAD_ADDRESS}
CPPFLAGS+=	-DSECONDARY_LOAD_ADDRESS=${SECONDARY_LOAD_ADDRESS}
CPPFLAGS+=	-DDISABLE_KEYPRESS

.if ${MACHINE_ARCH} == "x86_64"
LDFLAGS+=	-Wl,-m,elf_i386
AFLAGS+=	-m32
.endif

BUILDSYMLINKS+=	$S/arch/i386/include	machine \
		$S/arch/x86/include	x86

DPSRCS+=	machine x86

CLEANFILES+=	${PROG}.tmp

${PROG}: ${OBJS}
	${_MKTARGET_LINK}
	${CC} -o ${PROG}.tmp ${LDFLAGS} -Wl,-Ttext,${PRIMARY_LOAD_ADDRESS} \
	    ${OBJS}
	@ set -- $$( ${NM} -t d ${PROG}.tmp | grep 'free_space' \
		| ${TOOL_SED} 's/^0*//'  ); \
		echo "#### There are $$1 free bytes in ${PROG}"
	${OBJCOPY} -O binary ${PROG}.tmp ${PROG}
	rm -f ${PROG}.tmp

.include <bsd.prog.mk>
