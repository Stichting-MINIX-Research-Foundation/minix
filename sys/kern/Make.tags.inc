#	$NetBSD: Make.tags.inc,v 1.24 2011/01/21 23:23:44 dyoung Exp $
#
#	from: @(#)Make.tags.inc	8.1 (Berkeley) 6/11/93

# Common files for "make tags".
# Included by the Makefile for each architecture.

# Put the ../sys stuff near the end so that subroutine definitions win when
# there is a struct tag with the same name (eg., vmmeter).  The real
# solution would probably be for ctags to generate "struct vmmeter" tags.

.ifmake tags
# The invocation below returns every directory that contains sources, excluding
# the arch directories. Traditionally, COMM would have been an explicit list.
# This promises to be easier to maintain, considering how often the directory
# structure of the kernel sources has been changing recently.
SYSDIR?= ${.CURDIR:H:H}
FINDCOMM=	find -H ${SYSDIR} \( -path '*/dist/ipf' -o -name arch -o -name rump -o -name coda -o -name cxgb -o -name drm \) -prune -o -type f -name "*.[ch]" \( \! -name 'altq.h' \! -name 'nbcompat.h' \! -name 'pf_osfp.c' \! -name 'unichromereg.h' \! -name 'midway*' \! -name 'if_lmc.[ch]' \! -name 'aic79xxvar.h' \) -print | \
    sort -t / -u
COMM!=	${FINDCOMM}
.endif
