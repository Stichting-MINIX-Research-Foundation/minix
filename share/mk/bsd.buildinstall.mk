#	$NetBSD: bsd.buildinstall.mk,v 1.1 2012/11/15 23:51:53 joerg Exp $

#
# build_install logic for src/Makefile
# Used by src/lib/Makefile and src/tools/Makefile.
#
# Compute a list of subdirectories delimited by .WAIT.
# Run "make dependall && make install" for all subdirectories in a group
# concurrently, but wait after each group.
#
SUBDIR_GROUPS=	1
CUR_GROUP:=	1
.for dir in ${SUBDIR}
.  if ${dir} == ".WAIT"
CUR_GROUP:=	${CUR_GROUP}1
SUBDIR_GROUPS:=	${SUBDIR_GROUPS} ${CUR_GROUP}
.  else
SUBDIR_GROUP.${CUR_GROUP}+=	${dir}
.endif

.endfor

build_install:
.for group in ${SUBDIR_GROUPS}
.  if !empty(SUBDIR_GROUP.${group})
	${MAKEDIRTARGET} . ${SUBDIR_GROUP.${group}:C/^/dependall-/}
	${MAKEDIRTARGET} . ${SUBDIR_GROUP.${group}:C/^/install-/}
.  endif
.endfor
