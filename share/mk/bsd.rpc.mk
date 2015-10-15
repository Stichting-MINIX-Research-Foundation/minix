#	$NetBSD: bsd.rpc.mk,v 1.13 2013/12/15 00:28:45 christos Exp $

.include <bsd.init.mk>

RPC_XDIR?=	${.CURDIR}/
RPCGEN_FLAGS?=	-B

# We don't use implicit suffix rules here to avoid dependencies in the
# Installed files.

.if defined(RPC_INCS)						# {

.for I in ${RPC_INCS}
${I}: ${I:.h=.x}
	${_MKTARGET_CREATE}
	${TOOL_RPCGEN} ${RPCGEN_FLAGS} -h ${RPC_XDIR}${I:.h=.x} -o ${.TARGET}
.endfor

DPSRCS+=	${RPC_INCS}
CLEANFILES+=	${RPC_INCS}

.endif								# }


.if defined(RPC_XDRFILES)					# {

.for I in ${RPC_XDRFILES}
${I}: ${RPC_XDIR}${I:_xdr.c=.x}
	${_MKTARGET_CREATE}
	${TOOL_RPCGEN} ${RPCGEN_FLAGS} -c ${RPC_XDIR}${I:_xdr.c=.x} -o ${.TARGET}
.endfor

DPSRCS+=	${RPC_XDRFILES}
CLEANFILES+=	${RPC_XDRFILES}

.endif								# }


.if defined(RPC_SVCFILES)					# {

.for I in ${RPC_SVCCLASS}
_RPCS += -s ${I}
.endfor

.for I in ${RPC_SVCFILES}

${I}: ${RPC_XDIR}${I:_svc.c=.x}
	${_MKTARGET_CREATE}
	${TOOL_RPCGEN} ${RPCGEN_FLAGS} ${_RPCS} ${RPC_SVCFLAGS} ${RPC_XDIR}${I:_svc.c=.x} \
		-o ${.TARGET}
.endfor

DPSRCS+=	${RPC_SVCFILES}
CLEANFILES+=	${RPC_SVCFILES}

.endif								# }

.if defined(RPC_CLNTFILES)					# {

.for I in ${RPC_CLNTFILES}

${I}: ${RPC_XDIR}${I:_clnt.c=.x}
	${_MKTARGET_CREATE}
	${TOOL_RPCGEN} ${RPCGEN_FLAGS} -l ${_RPCS} ${RPC_CLNTFLAGS} \
		${RPC_XDIR}${I:_clnt.c=.x} -o ${.TARGET}
.endfor

DPSRCS+=	${RPC_CLNTFILES}
CLEANFILES+=	${RPC_CLNTFILES}

.endif								# }

##### Pull in related .mk logic
.include <bsd.obj.mk>
.include <bsd.sys.mk>
.include <bsd.clean.mk>
