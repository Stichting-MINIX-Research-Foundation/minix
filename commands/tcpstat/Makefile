PROG=	tcpstat
CPPFLAGS+= -I${NETBSDSRCDIR}/servers -D'ARGS(a)=a'

# LSC Force inet.h to use ioc_net.h
CPPFLAGS+= -D__NBSD_LIBC
MAN=

.include <bsd.prog.mk>
