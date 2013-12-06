# $NetBSD: Makefile,v 1.4 2003/10/26 07:25:34 lukem Exp $

.include <bsd.own.mk>
.include <bsd.sys.mk>		# for HOST_SH

M060SP=		${NETBSDSRCDIR}/sys/arch/m68k/060sp
ASM2GAS=	${M060SP}/asm2gas
FPLSP_SA=	${M060SP}/dist/fplsp.sa

all: fplsp.hex fplsp_wrap.S

fplsp_wrap.S Makefile.list: makeas.sh
	${HOST_SH} makeas.sh

fplsp.hex: ${FPLSP_SA} ${ASM2GAS} makeoffs.awk
	${HOST_SH} ${ASM2GAS} ${FPLSP_SA} | awk -f makeoffs.awk > fplsp.hex

.if exists(Makefile.list)
.include "Makefile.list"
.endif

clean:
	-rm ${ARCH_SRCS} ${ARCH_ADDS} fplsp.hex Makefile.list
