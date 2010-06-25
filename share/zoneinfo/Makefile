# @(#)Makefile	7.109

.include <bsd.own.mk>

# Change the line below for your time zone (after finding the zone you want in
# the time zone files, or adding it to a time zone file).
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -l rightzone
# to correct things.
# Use the command
#	make zonenames
# to get a list of the values you can use for LOCALTIME.

LOCALTIME=	GMT

# If you want something other than Eastern United States time as a template
# for handling POSIX-style time zone environment variables,
# change the line below (after finding the zone you want in the
# time zone files, or adding it to a time zone file).
# (When a POSIX-style environment variable is handled, the rules in the
# template file are used to determine "spring forward" and "fall back" days and
# times; the environment variable itself specifies UTC offsets of standard and
# summer time.)
# Alternately, if you discover you've got the wrong time zone, you can just
#	zic -p rightzone
# to correct things.
# Use the command
#	make zonenames
# to get a list of the values you can use for POSIXRULES.
# If you want POSIX compatibility, use "America/New_York".

POSIXRULES=	America/New_York

# Use an absolute path name for TZDIR unless you're just testing the software.

TZDIR=	${DESTDIR}/usr/share/zoneinfo

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

REDO=		posix_right

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

.PHONY:	posix_only
posix_only: ${TDATA}
	${_MKTARGET_CREATE}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZDIR} -L /dev/null ${TDATA}

.PHONY:	right_only
right_only: leapseconds ${TDATA}
	${_MKTARGET_CREATE}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZDDIR} -L leapseconds ${TDATA}

# In earlier versions of this makefile, the other two directories were
# subdirectories of $(TZDIR).  However, this led to configuration errors.
# For example, with posix_right under the earlier scheme,
# TZ='right/Australia/Adelaide' got you localtime with leap seconds,
# but gmtime without leap seconds, which led to problems with applications
# like sendmail that subtract gmtime from localtime.
# Therefore, the other two directories are now siblings of $(TZDIR).
# You must replace all of $(TZDIR) to switch from not using leap seconds
# to using them, or vice versa.
.PHONY:	other_two
other_two: leapseconds ${TDATA}
	${_MKTARGET_CREATE}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZDIR}-posix -L /dev/null ${TDATA}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} -d ${TZDIR}-leaps -L leapseconds ${TDATA}

.PHONY:	posix_right
posix_right: posix_only other_two

.PHONY:	right_posix
right_posix: right_only other_two

afterinstall: ${DATA} ${REDO} ${TABDATA}
	${_MKMSG_CREATE} ${POSIXRULES}
	cd ${.CURDIR} && \
	    ${TOOL_ZIC} -y ${YEARISTYPE} \
		-d ${TZDIR} -l ${LOCALTIME} -p ${POSIXRULES}
	for f in ${TABDATA}; do \
		${INSTALL_FILE} -o ${BINOWN} -g ${BINGRP} -m ${NONBINMODE} \
			${.CURDIR}/$$f ${DESTDIR}${TZDIR}; \
	done

.include <bsd.prog.mk>
