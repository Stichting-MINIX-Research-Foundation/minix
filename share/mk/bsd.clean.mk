# $NetBSD: bsd.clean.mk,v 1.5 2011/11/22 18:25:48 apb Exp $

# <bsd.clean.mk>
#
# Public targets:
#
# clean:	Delete files listed in ${CLEANFILES}.
# cleandir:	Delete files listed in ${CLEANFILES} and ${CLEANDIRFILES}.
#
# Public variables:
#
# CLEANFILES	Files to remove for both the clean and cleandir targets.
#
# CLEANDIRFILES	Files to remove for the cleandir target, but not for
#		the clean target.
#
# MKCLEANSRC	Whether or not to clean the source directory
# 		in addition to the object directory.
#
# MKCLEANVERIFY	Whether or not to verify that the file deletion worked.
#

.if !defined(_BSD_CLEAN_MK_)
_BSD_CLEAN_MK_=1

.include <bsd.init.mk>

MKCLEANSRC?=	yes
MKCLEANVERIFY?=	yes

clean:		.PHONY __doclean
__doclean:	.PHONY .MADE __cleanuse CLEANFILES
cleandir:	.PHONY clean __docleandir
__docleandir:	.PHONY .MADE __cleanuse CLEANDIRFILES

# __cleanuse is invoked with ${.ALLSRC} as the name of a variable
# (such as CLEANFILES or CLEANDIRFILES), or possibly a list of
# variable names.  ${.ALLSRC:@v@${${v}}@} will be the list of
# files to delete.  (We pass the variable name, e.g. CLEANFILES,
# instead of the file names, e.g. ${CLEANFILES}, because we don't
# want make to replace any of the file names with the result of
# searching .PATH.)
#
# If the list of files is empty, then the commands
# reduce to "true", with an "@" prefix to prevent echoing.
#
# The use of :M* is needed to handle the case that CLEANFILES
# or CLEANDIRFILES is not completely empty but contains spaces.
# This can easily happen when CLEANFILES or CLEANDIRFILES is set
# from other variables that happen to be empty.)
#
# The use of :Q is needed to handle the case that CLEANFILES
# or CLEANDIRFILES contains quoted strings, such as
# CLEANFILES = "filename with spaces".
#
__cleanuse: .USE
.if 0	# print "# clean CLEANFILES" for debugging
	${"${.ALLSRC:@v@${${v}:M*}@:Q}" == "":?@true:${_MKMSG} \
		"clean" ${.ALLSRC} }
.endif
.for _d in ${"${.OBJDIR}" == "${.CURDIR}" || "${MKCLEANSRC}" == "no" \
		:? ${.OBJDIR} \
		:  ${.OBJDIR} ${.CURDIR} }
	${"${.ALLSRC:@v@${${v}:M*}@:Q}" == "":?@true: \
	    (cd ${_d} && rm -f ${.ALLSRC:@v@${${v}}@} || true) }
.if "${MKCLEANVERIFY}" == "yes"
	@${"${.ALLSRC:@v@${${v}:M*}@:Q}" == "":?true: \
	    bad="\$(cd ${_d} && ls -d ${.ALLSRC:@v@${${v}}@} 2>/dev/null)"; \
	    if test -n "\$bad"; then \
	        echo "Failed to remove files from ${_d}:" ; \
	        echo "\$bad" ; \
	        false ; \
	    fi }
.endif
.endfor

# Don't automatically load ".depend" files during "make clean"
# or "make cleandir".
.if make(clean) || make(cleandir)
.MAKE.DEPENDFILE := .depend.no-such-file
.endif

.endif	# !defined(_BSD_CLEAN_MK)
