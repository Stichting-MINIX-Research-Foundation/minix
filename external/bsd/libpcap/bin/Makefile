#	$NetBSD: Makefile,v 1.2 2010/12/05 09:09:39 mrg Exp $

.include <bsd.own.mk>

.PATH: ${.CURDIR}/../dist

SCRIPTSDIR=/usr/bin
SCRIPTS=pcap-config

MAN=pcap-config.1

pcap-config: pcap-config.in
	@rm -f ${.TARGET}
	${TOOL_SED} -e 's,@libdir@,/usr/lib,g' \
	    -e 's,@includedir@,/usr/include,g' \
	    -e 's,@LIBS@,,g' < ${.ALLSRC} > ${.TARGET}
	chmod a+x ${.TARGET}

CLEANFILES+=	pcap-config

NOPROG=
		
.include <bsd.prog.mk>
