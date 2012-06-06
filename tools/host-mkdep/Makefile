#	$NetBSD: Makefile,v 1.10 2008/10/19 22:05:23 apb Exp $

TIMESTAMP=	${TOOLDIR}/bin/${_TOOL_PREFIX}host-mkdep

CLEANFILES+=	config.cache config.log config.status host-mkdep

.include <bsd.hostprog.mk>

# When host-mkdep is built, TOOL_AWK is not yet available, so we do not
# pass AWK=${TOOL_AWK:Q} to configure; we allow configure to find awk
# for itself (or complain if it can't find awk).

realall: host-mkdep
host-mkdep: configure host-mkdep.in
	-rm -f $@
	CC=${HOST_CC:Q} ${HOST_SH} ${.CURDIR}/configure --cache-file=config.cache
	chmod +x $@

# This is the only program that comes before binstall.
install: ${TIMESTAMP}
${TIMESTAMP}: host-mkdep
	mkdir -p ${TOOLDIR}/bin
	cp host-mkdep $@
	chmod +x $@

# Run by hand, then "configure" script committed:
regen:
	cd ${.CURDIR} && ${TOOLDIR}/bin/${_TOOL_PREFIX}autoconf
