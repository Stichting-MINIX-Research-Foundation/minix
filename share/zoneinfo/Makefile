#	$NetBSD: Makefile,v 1.43 2009/04/10 16:16:12 apb Exp $

.include <bsd.own.mk>

# Change the line below for your time zone (after finding the zone you want in
# the time zone files, or adding it to a time zone file).
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -l rightzone

# If you want something other than Eastern United States time as a template
# for handling POSIX-style time zone environment variables,
# change the line below (after finding the zone you want in the
# time zone files, or adding it to a time zone file).
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -p rightzone

POSIXRULES=	US/Pacific

# Use an absolute path name for TZDIR unless you're just testing the software.
# Note: ${DESTDIR} is prepended to this for the actual copy.

TZDIR=	/usr/share/zoneinfo

# If you always want time values interpreted as "seconds since the epoch
# (not counting leap seconds)", use
# 	REDO=		posix_only
# below.  If you always want right time values interpreted as "seconds since
# the epoch" (counting leap seconds)", use
#	REDO=		right_only
# below.  If you want both sets of data available, with leap seconds not
# counted normally, use
#	REDO=		posix_right
# below.  If you want both sets of data available, with leap seconds counted
# normally, use
#	REDO=		right_posix
# below.

REDO=		posix_only

# Since "." may not be in PATH...
YEARISTYPE=	"${HOST_SH} ${.CURDIR}/yearistype.sh"

YDATA=		africa antarctica asia australasia \
		europe northamerica southamerica pacificnew etcetera factory \
		backward
NDATA=		systemv
SDATA=		solar87 solar88 solar89
TDATA=		$(YDATA) $(NDATA) $(SDATA)
TABDATA=	iso3166.tab zone.tab
DATA=		$(YDATA) $(NDATA) $(SDATA) leapseconds # yearistype.sh
USNO=		usno1988 usno1989

TZBUILDDIR=	${.OBJDIR}/builddir

.PHONY:	posix_only
posix_only: ${TDATA}
	${_MKTARGET_CREATE}
	mkdir -p ${TZBUILDDIR}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR} -L /dev/null ${TDATA}

.PHONY:	right_only
right_only: leapseconds ${TDATA}
	${_MKTARGET_CREATE}
	mkdir -p ${TZBUILDDIR}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR} -L leapseconds ${TDATA}

.PHONY:	other_two
other_two: leapseconds ${TDATA}
	${_MKTARGET_CREATE}
	mkdir -p ${TZBUILDDIR}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR}/posix -L /dev/null ${TDATA}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR}/right -L leapseconds ${TDATA}

.PHONY:	posix_right
posix_right: posix_only other_two

.PHONY:	right_posix
right_posix: right_only other_two

.if ${MKUNPRIVED} == "no"
TOOL_PAX.unpriv=-pe
.else
TOOL_PAX.unpriv=
.endif

.if ${MKSHARE} != "no"
afterinstall: ${DATA} ${REDO} ${TABDATA}
	${_MKMSG_CREATE} ${POSIXRULES}
	mkdir -p ${TZBUILDDIR}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR} -p ${POSIXRULES}
	cd ${TZBUILDDIR} && \
	    find . -type f -print | xargs ${TOOL_STAT} -qf '%d,%i %N' | sort \
	| while read devino tzfile; do \
		destfile=${DESTDIR}${TZDIR}/$${tzfile#./}; \
		if [ "$$devino" = "$$lastdevino" ]; then \
			if  ldevino=`${TOOL_STAT} -qf '%d %i' $$lastfile` && \
			    tdevino=`${TOOL_STAT} -qf '%d %i' $$destfile` && \
			    [ "$$ldevino" = "$$tdevino" ]; then \
				continue; \
			fi; \
			${_MKSHMSG_INSTALL} $$destfile; \
			${_MKSHECHO} ${INSTALL_LINK} -o ${BINOWN} -g ${BINGRP} \
			    -m ${NONBINMODE} $$lastfile $$destfile; \
			${INSTALL_LINK} -o ${BINOWN} -g ${BINGRP} \
			    -m ${NONBINMODE} $$lastfile $$destfile; \
		else \
			lastdevino=$$devino; \
			lastfile=$$destfile; \
			cmp -s $$tzfile $$destfile >/dev/null 2>&1 && continue;\
			${_MKSHMSG_INSTALL} $$destfile; \
			${_MKSHECHO} ${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} \
			    -m ${NONBINMODE} $$tzfile $$destfile; \
			${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} \
			    -m ${NONBINMODE} $$tzfile $$destfile; \
		fi; \
	done
	for f in ${TABDATA}; do \
		${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
			${.CURDIR}/$$f ${DESTDIR}${TZDIR}; \
	done
.else	# ${MKSHARE} == "no"
afterinstall:
.endif	# ${MKSHARE} == "no"

clean:
	-rm -rf ${TZBUILDDIR}

.include <bsd.prog.mk>
