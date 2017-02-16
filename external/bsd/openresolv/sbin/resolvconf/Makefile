# $NetBSD: Makefile,v 1.6 2013/07/12 17:32:47 roy Exp $
#

.include <bsd.own.mk>

DIST=		${NETBSDSRCDIR}/external/bsd/openresolv/dist
.PATH:		${DIST} 

SCRIPTS=	resolvconf
SCRIPTSDIR=	/sbin

FILES=		libc dnsmasq named unbound
FILESDIR=	/libexec/resolvconf
# We should not have to do this ...
.for f in ${FILES}
FILESBUILD_$f=	yes
.endfor

MAN=		resolvconf.conf.5 resolvconf.8

CLEANFILES=	${SCRIPTS} ${FILES} ${MAN}

CMD1=		\1 status >/dev/null 2>\&1
CMD2=		\1 restart
RESTARTCMD=	if /etc/rc.d/${CMD1}; then /etc/rc.d/${CMD2}; fi
.for f in ${SCRIPTS} ${FILES} ${MAN}
${f}:	Makefile ${f}.in
	${TOOL_SED} -e 's:@PREFIX@::g' \
		-e 's:@SBINDIR@:/sbin:g' \
		-e 's:@RCDIR@:/etc/rc.d:g' \
		-e 's:@SYSCONFDIR@:/etc:g' \
		-e 's:@LIBEXECDIR@:/libexec/resolvconf:g' \
		-e 's:@VARDIR@:/var/run/resolvconf:g' \
		-e 's:@RESTARTCMD \(.*\)@:${RESTARTCMD}:g' \
		${DIST}/${f}.in > $@
.endfor

.include <bsd.prog.mk>
