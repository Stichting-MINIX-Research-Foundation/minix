# $NetBSD: include-main.mk,v 1.6 2021/01/22 00:44:55 rillig Exp $
#
# Until 2020-09-05, the .INCLUDEDFROMFILE magic variable did not behave
# as described in the manual page.
#
# The manual page says that it is the "filename of the file this Makefile
# was included from", while before 2020-09-05 it was the "filename in which
# the latest .include happened". See parse.c, function ParseSetIncludeFile.
#
# Since 2020-09-05, the .INCLUDEDFROMDIR and .INCLUDEDFROMFILE variables
# properly handle nested includes and even .for loops.

.if !defined(.INCLUDEDFROMFILE)
.  info main-before-ok
.else
.  warning main-before-fail(${.INCLUDEDFROMFILE})
.endif

.for i in once
.  if !defined(.INCLUDEDFROMFILE)
.    info main-before-for-ok
.  else
.    warning main-before-for-fail(${.INCLUDEDFROMFILE})
.  endif
.endfor

.include "include-sub.mk"

.if !defined(.INCLUDEDFROMFILE)
.  info main-after-ok
.else
.  warning main-after-fail(${.INCLUDEDFROMFILE})
.endif

.for i in once
.  if !defined(.INCLUDEDFROMFILE)
.    info main-after-for-ok
.  else
.    warning main-after-for-fail(${.INCLUDEDFROMFILE})
.  endif
.endfor

all:	# nothing
