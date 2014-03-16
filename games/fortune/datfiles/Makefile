#	$NetBSD: Makefile,v 1.38 2008/09/26 18:23:09 apb Exp $
#	@(#)Makefile	8.2 (Berkeley) 4/19/94

.include <bsd.own.mk>			# for INSTALL_OFFENSIVE_FORTUNES

SRCFILES=	fortunes fortunes2 startrek zippy unamerican-o limerick \
		netbsd farber netbsd-tips
DATFILES=	fortunes.dat fortunes2.dat startrek.dat zippy.dat \
		fortunes-o fortunes-o.dat fortunes2-o fortunes2-o.dat \
		limerick.dat limerick-o limerick-o.dat unamerican-o \
		unamerican-o.dat netbsd.dat netbsd-o netbsd-o.dat \
		farber.dat netbsd-tips.dat

# TO AVOID INSTALLING THE POTENTIALLY OFFENSIVE FORTUNES, RUN 'make' with
# "INSTALL_OFFENSIVE_FORTUNES=NO", or set "INSTALL_OFFENSIVE_FORTUNES=NO"
# IN bsd.own.mk OR /etc/mk.conf.

INSTALL_OFFENSIVE_FORTUNES?=	YES
.if (${INSTALL_OFFENSIVE_FORTUNES} == "YES")
TYPE=		real
.else
TYPE=		fake
.endif

CLEANFILES+=${DATFILES}

STRFILE=${TOOL_STRFILE}

.if ${MKSHARE} != "no"
FILES=${SRCFILES} ${DATFILES}
FILESDIR=/usr/share/games/fortune
.endif

realall: ${DATFILES}

# Normal fortunes: build directly from data files.

fortunes.dat fortunes2.dat startrek.dat zippy.dat limerick.dat netbsd.dat farber.dat netbsd-tips.dat:
	${_MKTARGET_CREATE}
	${STRFILE} -rs ${.CURDIR}/${.TARGET:R} ${.TARGET}
fortunes.dat:	fortunes
fortunes2.dat:	fortunes2
startrek.dat:	startrek
zippy.dat:	zippy
limerick.dat:	limerick
netbsd.dat:	netbsd
farber.dat:	farber
netbsd-tips.dat:	netbsd-tips

# Obscene fortunes: we have to build source and data files

fortunes-o: fortunes-o.${TYPE}
	${_MKTARGET_CREATE}
	tr 'a-zA-Z' 'n-za-mN-ZA-M' < ${.ALLSRC} > ${.TARGET}
fortunes-o.dat: fortunes-o
	${_MKTARGET_CREATE}
	${STRFILE} -rsx ${.ALLSRC} ${.TARGET}

fortunes2-o: fortunes2-o.${TYPE}
	${_MKTARGET_CREATE}
	tr 'a-zA-Z' 'n-za-mN-ZA-M' < ${.ALLSRC} > ${.TARGET}
fortunes2-o.dat: fortunes2-o
	${_MKTARGET_CREATE}
	${STRFILE} -rsx ${.ALLSRC} ${.TARGET}

limerick-o: limerick-o.${TYPE}
	${_MKTARGET_CREATE}
	tr 'a-zA-Z' 'n-za-mN-ZA-M' < ${.ALLSRC} > ${.TARGET}
limerick-o.dat: limerick-o
	${_MKTARGET_CREATE}
	${STRFILE} -rsx ${.ALLSRC} ${.TARGET}

unamerican-o: unamerican-o.${TYPE}
	${_MKTARGET_CREATE}
	tr 'a-zA-Z' 'n-za-mN-ZA-M' < ${.ALLSRC} > ${.TARGET}
unamerican-o.dat: unamerican-o
	${_MKTARGET_CREATE}
	${STRFILE} -rsx ${.ALLSRC} ${.TARGET}

netbsd-o: netbsd-o.${TYPE}
	${_MKTARGET_CREATE}
	tr 'a-zA-Z' 'n-za-mN-ZA-M' < ${.ALLSRC} > ${.TARGET}
netbsd-o.dat: netbsd-o
	${_MKTARGET_CREATE}
	${STRFILE} -rsx ${.ALLSRC} ${.TARGET}


.include <bsd.prog.mk>
