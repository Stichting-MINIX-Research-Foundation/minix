# $Id: misc.mk,v 1.1 2014/08/21 13:44:51 apb Exp $

.if !exists(${.CURDIR}/)
.warning ${.CURDIR}/ doesn't exist ?
.endif

.if !exists(${.CURDIR}/.)
.warning ${.CURDIR}/. doesn't exist ?
.endif

.if !exists(${.CURDIR}/..)
.warning ${.CURDIR}/.. doesn't exist ?
.endif

all:
	@: all is well
