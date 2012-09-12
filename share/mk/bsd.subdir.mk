#	$NetBSD: bsd.subdir.mk,v 1.52 2010/05/26 00:48:15 uwe Exp $
#	@(#)bsd.subdir.mk	8.1 (Berkeley) 6/8/93

.include <bsd.init.mk>

.if !defined(NOSUBDIR)					# {

.for dir in ${SUBDIR}
.if "${dir}" == ".WAIT"
# Don't play with .WAIT
__REALSUBDIR+=${dir}
.else
.if "${dir:H}" != ""
# It is a relative path; make it absolute so exists can't search the path.
.if exists(${.CURDIR}/${dir}.${MACHINE})
__REALSUBDIR+=${dir}.${MACHINE}
.else
__REALSUBDIR+=${dir}
.endif
.else
# It is an absolute path; leave it alone
.if exists(${dir}.${MACHINE})
__REALSUBDIR+=${dir}.${MACHINE}
.else
__REALSUBDIR+=${dir}
.endif
.endif
.endif
.endfor

__recurse: .USE
	@${MAKEDIRTARGET} ${.TARGET:C/^[^-]*-//} ${.TARGET:C/-.*$//}

.if make(cleandir)
__RECURSETARG=	${TARGETS:Nclean}
clean:
.else
__RECURSETARG=	${TARGETS}
.endif

# for obscure reasons, we can't do a simple .if ${dir} == ".WAIT"
# but have to assign to __TARGDIR first.
.for targ in ${__RECURSETARG}
.for dir in ${__REALSUBDIR}
__TARGDIR := ${dir}
.if ${__TARGDIR} == ".WAIT"
SUBDIR_${targ} += .WAIT
.elif !commands(${targ}-${dir})
${targ}-${dir}: .PHONY .MAKE __recurse
SUBDIR_${targ} += ${targ}-${dir}
.endif
.endfor
subdir-${targ}: .PHONY ${SUBDIR_${targ}}
${targ}: subdir-${targ}
.endfor

.endif	# ! NOSUBDIR					# }

${TARGETS}:	# ensure existence
