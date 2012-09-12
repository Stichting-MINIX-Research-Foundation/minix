##
##  @(#)Makefile	1.00	Jan. 11, 2000
##
##  Makefile for syslogd/klogd

PROG=	syslogd
PIDFILE= -DPIDFILE=\"/var/run/syslogd.pid\"
CPPFLAGS+= -Ddebug=0 ${PIDFILE}
CPPFLAGS+= -D__NBSD_LIBC
BINMODE= 700
MAN=

.include <bsd.prog.mk>
