#	$NetBSD: bsd.klinks.mk,v 1.6 2009/11/27 13:50:29 pooka Exp $
#

.include <bsd.own.mk>

##### Default values
.if !defined(S)
.if defined(NETBSDSRCDIR)
S=	${NETBSDSRCDIR}/sys
.elif defined(BSDSRCDIR)
S=	${BSDSRCDIR}/sys
.else
S=	/sys
.endif
.endif

CLEANFILES+=	machine ${MACHINE_CPU}
.if ${MACHINE} == "sun2" || ${MACHINE} == "sun3"
CLEANFILES+=	sun68k
.elif ${MACHINE} == "sparc64"
CLEANFILES+=	sparc
.elif ${MACHINE} == "i386"
CLEANFILES+=	x86
.elif ${MACHINE} == "amd64"
CLEANFILES+=	x86
.endif

.if defined(XEN_BUILD) || ${MACHINE} == "xen"
CLEANFILES+=	xen xen-ma/machine # xen-ma
CPPFLAGS+=	-I${.OBJDIR}/xen-ma
.if ${MACHINE_CPU} == "i386"
CLEANFILES+=	x86
.endif
.endif

# XXX.  This should be done a better way.  It's @'d to reduce visual spew.
# XXX   .BEGIN is used to make sure the links are done before anything else.
.if !make(obj) && !make(clean) && !make(cleandir)
.BEGIN:
	@rm -f machine && \
	    ln -s $S/arch/${MACHINE}/include machine
	@rm -f ${MACHINE_CPU} && \
	    ln -s $S/arch/${MACHINE_CPU}/include ${MACHINE_CPU}
# XXX. it gets worse..
.if ${MACHINE} == "sun2" || ${MACHINE} == "sun3"
	@rm -f sun68k && \
	    ln -s $S/arch/sun68k/include sun68k
.endif
.if ${MACHINE} == "sparc64"
	@rm -f sparc && \
	    ln -s $S/arch/sparc/include sparc
.endif
.if ${MACHINE} == "amd64"
	@rm -f x86 && \
	    ln -s $S/arch/x86/include x86
	@rm -f i386 && \
	    ln -s $S/arch/i386/include i386
.endif
.if ${MACHINE_CPU} == "i386"
	@rm -f x86 && \
	    ln -s $S/arch/x86/include x86
.endif
.if defined(XEN_BUILD) || ${MACHINE} == "xen"
	@rm -f xen && \
	    ln -s $S/arch/xen/include xen
	@rm -rf xen-ma && mkdir xen-ma && \
	    ln -s ../${XEN_BUILD:U${MACHINE_ARCH}} xen-ma/machine
.endif
.endif
