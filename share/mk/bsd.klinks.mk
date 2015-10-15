#	$NetBSD: bsd.klinks.mk,v 1.13 2014/08/10 05:57:31 matt Exp $
#

.include <bsd.own.mk>

KLINK_MACHINE?=	${MACHINE}

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

CLEANFILES+=	machine ${MACHINE_CPU} ${KLINK_MACHINE}
.if ${KLINK_MACHINE} == "sun2" || ${KLINK_MACHINE} == "sun3"
CLEANFILES+=	sun68k
.elif ${KLINK_MACHINE} == "sparc64"
CLEANFILES+=	sparc
.elif ${KLINK_MACHINE} == "i386"
CLEANFILES+=	x86
.elif ${KLINK_MACHINE} == "amd64"
CLEANFILES+=	x86 i386
.elif ${KLINK_MACHINE} == "evbmips"
CLEANFILES+=	algor sbmips
.elif ${MACHINE_CPU} == "aarch64"
CLEANFILES+=	arm
.endif

.if defined(XEN_BUILD) || ${KLINK_MACHINE} == "xen"
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
	-@rm -f machine && \
	    ln -s $S/arch/${KLINK_MACHINE}/include machine
	-@rm -f ${KLINK_MACHINE} && \
	    ln -s $S/arch/${KLINK_MACHINE}/include ${KLINK_MACHINE}
	-@if [ -d $S/arch/${MACHINE_CPU} ]; then \
	    rm -f ${MACHINE_CPU} && \
	    ln -s $S/arch/${MACHINE_CPU}/include ${MACHINE_CPU}; \
	 fi
# XXX. it gets worse..
.if ${KLINK_MACHINE} == "sun2" || ${KLINK_MACHINE} == "sun3"
	-@rm -f sun68k && \
	    ln -s $S/arch/sun68k/include sun68k
.endif
.if ${KLINK_MACHINE} == "sparc64"
	-@rm -f sparc && \
	    ln -s $S/arch/sparc/include sparc
.endif
.if ${KLINK_MACHINE} == "amd64"
	-@rm -f i386 && \
	    ln -s $S/arch/i386/include i386
.endif
.if ${MACHINE_CPU} == "i386" || ${MACHINE_CPU} == "x86_64"
	-@rm -f x86 && \
	    ln -s $S/arch/x86/include x86
.endif
.if ${MACHINE_CPU} == "aarch64"
	-@rm -f arm && \
	    ln -s $S/arch/arm/include arm
.endif
.if defined(XEN_BUILD) || ${KLINK_MACHINE} == "xen"
	-@rm -f xen && \
	    ln -s $S/arch/xen/include xen
	-@rm -rf xen-ma && mkdir xen-ma && \
	    ln -s ../${XEN_BUILD:U${MACHINE_ARCH}} xen-ma/machine
.endif
.if ${KLINK_MACHINE} == "evbmips"
	-@rm -f algor && \
	    ln -s $S/arch/algor/include algor
	-@rm -f sbmips && \
	    ln -s $S/arch/sbmips/include sbmips
.endif
.endif
