#	$NetBSD: bsd.kinc.mk,v 1.36 2006/03/16 18:43:34 jwise Exp $

# Variables:
#
# INCSDIR	Directory to install includes into (and/or make, and/or
#		symlink, depending on what's going on).
#
# INCS		Headers to install.
#
# DEPINCS	Headers to install which are built dynamically.
#
# SUBDIR	Subdirectories to enter
#
# INCSYMLINKS	Symlinks to make (unconditionally), a la bsd.links.mk.
#		Note that the original bits will be 'rm -rf'd rather than
#		just 'rm -f'd, to make the right thing happen with include
#		directories.
#

.include <bsd.init.mk>

##### Basic targets
.PRECIOUS:	${DESTDIR}${INCSDIR}
includes:	${DESTDIR}${INCSDIR} .WAIT ${INCS} incinstall

##### Install rules
incinstall::	# ensure existence
.PHONY:		incinstall

# make sure the directory is OK, and install includes.

${DESTDIR}${INCSDIR}: .EXEC
	@if [ ! -d ${.TARGET} ] || [ -h ${.TARGET} ] ; then \
		${_MKSHMSG_CREATE} ${.TARGET}; \
		/bin/rm -rf ${.TARGET}; \
		${_MKSHECHO} ${INSTALL_DIR} -o ${BINOWN} -g ${BINGRP} -m 755 \
			${.TARGET}; \
		${INSTALL_DIR} -o ${BINOWN} -g ${BINGRP} -m 755 \
			${.TARGET}; \
	fi

# -c is forced on here, in order to preserve modtimes for "make depend"
__incinstall: .USE
	@cmp -s ${.ALLSRC} ${.TARGET} > /dev/null 2>&1 || \
	    (${_MKSHMSG_INSTALL} ${.TARGET}; \
	     ${_MKSHECHO} "${INSTALL_FILE} -c -o ${BINOWN} -g ${BINGRP} \
		-m ${NONBINMODE} ${.ALLSRC} ${.TARGET}" && \
	     ${INSTALL_FILE} -c -o ${BINOWN} -g ${BINGRP} \
		-m ${NONBINMODE} ${.ALLSRC} ${.TARGET})

.for F in ${INCS:O:u} ${DEPINCS:O:u}
_F:=		${DESTDIR}${INCSDIR}/${F}		# installed path

.if ${MKUPDATE} == "no"
${_F}!		${F} __incinstall			# install rule
.else
${_F}:		${F} __incinstall			# install rule
.endif

incinstall::	${_F}
.PRECIOUS:	${_F}					# keep if install fails
.endfor

.undef _F

.if defined(INCSYMLINKS) && !empty(INCSYMLINKS)
incinstall::
	@(set ${INCSYMLINKS}; \
	 while test $$# -ge 2; do \
		l=$$1; shift; \
		t=${DESTDIR}$$1; shift; \
		if  ttarg=`${TOOL_STAT} -qf '%Y' $$t` && \
		    [ "$$l" = "$$ttarg" ]; then \
			continue ; \
		fi ; \
		${_MKSHMSG_INSTALL} $$t; \
		${_MKSHECHO} ${INSTALL_SYMLINK} $$l $$t; \
		${INSTALL_SYMLINK} $$l $$t; \
	 done; )
.endif

##### Pull in related .mk logic
.include <bsd.subdir.mk>
.include <bsd.sys.mk>
