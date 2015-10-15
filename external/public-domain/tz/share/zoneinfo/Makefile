#	$NetBSD: Makefile,v 1.2 2014/08/08 09:17:03 apb Exp $

.include <bsd.own.mk>
.include "../../Makefile.inc"

.PATH: ${TZDISTDIR}

# If you want something other than Eastern United States time as a template
# for handling POSIX-style time zone environment variables,
# change the line below (after finding the zone you want in the
# time zone files, or adding it to a time zone file).
# (When a POSIX-style environment variable is handled, the rules in the
# template file are used to determine "spring forward" and "fall back" days and
# times; the environment variable itself specifies UT offsets of standard and
# summer time.)
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -p rightzone
# to correct things.
# Use the command
#	make zonenames
# to get a list of the values you can use for POSIXRULES.
# If you want POSIX compatibility, use "America/New_York".

POSIXRULES=	America/New_York

# "Compiled" time zone information is placed in the "TZDIR" directory
# (and subdirectories).
# Use an absolute path name for TZDIR unless you're just testing the software.
# Note: ${DESTDIR} is prepended to this for the actual copy.

TZDIR=	/usr/share/zoneinfo

# If you always want time values interpreted as "seconds since the epoch
# (not counting leap seconds)", use
#	REDO=		posix_only
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
YEARISTYPE=	"${HOST_SH} ${TZDISTDIR}/yearistype.sh"

PRIMARY_YDATA=	africa antarctica asia australasia \
		europe northamerica southamerica
YDATA=		$(PRIMARY_YDATA) pacificnew etcetera backward
NDATA=		systemv factory
TDATA=		$(YDATA) $(NDATA)
ZONETABLES=	zone1970.tab zone.tab
TABDATA=	iso3166.tab leapseconds $(ZONETABLES)
DATA=		$(YDATA) $(NDATA) $(TABDATA) # yearistype.sh

TZBUILDDIR=	${.OBJDIR}/builddir

.PHONY:	posix_only
posix_only: ${TDATA}
	${_MKTARGET_CREATE}
	mkdir -p ${TZBUILDDIR}
	cd ${TZDISTDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR} -L /dev/null ${TDATA}

.PHONY:	right_only
right_only: leapseconds ${TDATA}
	${_MKTARGET_CREATE}
	mkdir -p ${TZBUILDDIR}
	cd ${TZDISTDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR} -L leapseconds ${TDATA}

.PHONY:	other_two
other_two: leapseconds ${TDATA}
	${_MKTARGET_CREATE}
	mkdir -p ${TZBUILDDIR}
	cd ${TZDISTDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR}/posix -L /dev/null ${TDATA}
	cd ${TZDISTDIR} && \
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
	cd ${TZDISTDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZBUILDDIR} -p ${POSIXRULES}
	cd ${TZBUILDDIR} && \
	    find . -type f -print | xargs ${TOOL_STAT} -qf '%d,%i %N' | sort \
	| while read devino tzfile; do \
		destfile=${DESTDIR}${TZDIR}/$${tzfile#./}; \
		destdir=$$(dirname $${destfile}); \
		if [ ! -d "$$destdir" ]; then \
			${_MKSHMSG_INSTALL} $$destdir; \
			${_MKSHECHO} ${INSTALL_DIR} -o ${BINOWN} -g ${BINGRP} \
			$$destdir; \
			${INSTALL_DIR} -o ${BINOWN} -g ${BINGRP} $$destdir; \
		fi; \
		if [ "$$devino" = "$$lastdevino" ]; then \
			if  ldevino=$$(${TOOL_STAT} -qf '%d %i' $$lastfile) && \
			    tdevino=$$(${TOOL_STAT} -qf '%d %i' $$destfile) && \
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
			${TZDISTDIR}/$$f ${DESTDIR}${TZDIR}; \
	done
.else	# ${MKSHARE} == "no"
afterinstall:
.endif	# ${MKSHARE} == "no"

clean:
	-rm -rf ${TZBUILDDIR}

.include <bsd.prog.mk>
