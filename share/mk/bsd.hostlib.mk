#	$NetBSD: bsd.hostlib.mk,v 1.19 2014/12/01 01:34:30 erh Exp $

.include <bsd.init.mk>
.include <bsd.sys.mk>

##### Basic targets

##### Default values
CFLAGS+=	${COPTS}
MKDEP_SUFFIXES?=	.o .lo .d

# Override these:
MKDEP:=		${HOST_MKDEP}
MKDEPCXX:=	${HOST_MKDEPCXX}

.if ${TOOLCHAIN_MISSING} == "no" || defined(EXTERNAL_TOOLCHAIN)
OBJHOSTMACHINE=	# set
.endif

##### Build rules
.if defined(HOSTLIB)
_YHLSRCS=	${SRCS:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS:M*.y:.y=.h}}
DPSRCS+=	${_YHLSRCS}
CLEANFILES+=	${_YHLSRCS}
.endif	# defined(HOSTLIB)

.if !empty(SRCS:N*.h:N*.sh)
OBJS+=		${SRCS:N*.h:N*.sh:R:S/$/.lo/g}
.endif

.if defined(OBJS) && !empty(OBJS)
.NOPATH: lib${HOSTLIB}.a ${OBJS} ${_YHLSRCS}

${OBJS}: ${DPSRCS}

lib${HOSTLIB}.a: ${OBJS} ${DPADD}
	${_MKTARGET_BUILD}
	rm -f ${.TARGET}
	${HOST_AR} cq ${.TARGET} ${OBJS}
	${HOST_RANLIB} ${.TARGET}

.endif	# defined(OBJS) && !empty(OBJS)

realall: lib${HOSTLIB}.a

CLEANFILES+= a.out [Ee]rrs mklog core *.core lib${HOSTLIB}.a ${OBJS}

beforedepend:
CFLAGS:=	${HOST_CFLAGS}
CPPFLAGS:=	${HOST_CPPFLAGS}

##### Pull in related .mk logic
.include <bsd.obj.mk>
.include <bsd.dep.mk>
.include <bsd.clean.mk>

${TARGETS}:	# ensure existence
