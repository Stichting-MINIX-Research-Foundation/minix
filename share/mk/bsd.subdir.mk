#	$NetBSD: bsd.subdir.mk,v 1.50 2009/11/29 16:00:00 uebayasi Exp $
#	@(#)bsd.subdir.mk	8.1 (Berkeley) 6/8/93

.include <bsd.init.mk>

# MINIX: cleandepend works for SUBDIRs
TARGETS+= cleandepend
.PHONY: cleandepend
.NOTMAIN: cleandepend

.if !defined(NOSUBDIR)					# {

.for dir in ${SUBDIR}
.if exists(${dir}.${MACHINE})
__REALSUBDIR+=${dir}.${MACHINE}
.else
__REALSUBDIR+=${dir}
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
