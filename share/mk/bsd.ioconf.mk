#	$NetBSD: bsd.ioconf.mk,v 1.3 2010/03/25 20:37:36 pooka Exp $
#

.include <bsd.own.mk>

# If IOCONF is defined, autocreate ioconf.[ch] and locators.h.
# This is useful mainly for devices.
.if !empty(IOCONF)

# discourage direct inclusion.  bsd.ioconf.mk will hopefully go away
# when the kernel build procedures are unified.
.if defined(_BSD_IOCONF_MK_USER_)

# XXX: ioconf.c doesn't need to depend on TOOL_CONFIG, but that helps
# keep builds working while hashing out some of the experimental
# features related to ioconf.
.if ${USETOOLS} == "yes"
CONFIGDEP=${TOOL_CONFIG}
.endif
ioconf.c: ${IOCONF} ${CONFIGDEP}
	${TOOL_CONFIG} -b ${.OBJDIR} -s ${S} ${.CURDIR}/${IOCONF}
	# config doesn't change the files if they're unchanged.  however,
	# here we want to satisfy our make dependency, so force a
	# timestamp update
	touch ioconf.c ioconf.h locators.h

.else # _BSD_IOCONF_MK_USER_

ioconf.c:
	@echo do not include bsd.ioconf.mk directly
	@false

.endif # _BSD_IOCONF_MK_USER_

locators.h: ioconf.c
ioconf.h: ioconf.c

CLEANFILES+= ioconf.c ioconf.h locators.h
DPSRCS+= ioconf.c ioconf.h locators.h
.endif
