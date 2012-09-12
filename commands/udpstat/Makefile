PROG=	udpstat
CPPFLAGS+= -I${NETBSDSRCDIR}/servers -D'ARGS(a)=a'
MAN=

# LSC Force inet.h to use sys/ioc_net.h
CPPFLAGS+= -D__NBSD_LIBC

.include <bsd.prog.mk>
