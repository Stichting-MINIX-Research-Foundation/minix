#	$NetBSD: bsd.kmodule.mk,v 1.37 2012/08/10 16:34:23 joerg Exp $

# We are not building this with PIE
MKPIE=no

.include <bsd.init.mk>
.include <bsd.klinks.mk>
.include <bsd.sys.mk>

##### Basic targets
realinstall:	kmodinstall

KERN=		$S/kern

CFLAGS+=	-ffreestanding ${COPTS}
CPPFLAGS+=	-nostdinc -I. -I${.CURDIR} -isystem $S -isystem $S/arch
CPPFLAGS+=	-isystem ${S}/../common/include
CPPFLAGS+=	-D_KERNEL -D_LKM -D_MODULE -DSYSCTL_INCLUDE_DESCR

# XXX until the kernel is fixed again...
CFLAGS+=	-fno-strict-aliasing -Wno-pointer-sign

# XXX This is a workaround for platforms that have relative relocations
# that, when relocated by the module loader, result in addresses that
# overflow the size of the relocation (e.g. R_PPC_REL24 in powerpc).
# The real solution to this involves generating trampolines for those
# relocations inside the loader and removing this workaround, as the
# resulting code would be much faster.
.if ${MACHINE_CPU} == "arm"
CFLAGS+=	-mlong-calls
.elif ${MACHINE_CPU} == "powerpc"
CFLAGS+=	-mlongcall
.elif ${MACHINE_CPU} == "vax"
CFLAGS+=	-fno-pic
.endif

# evbppc needs some special help
.if ${MACHINE} == "evbppc"

. ifndef PPC_INTR_IMPL
PPC_INTR_IMPL=\"powerpc/intr.h\"
. endif
. ifndef PPC_PCI_MACHDEP_IMPL
PPC_PCI_MACHDEP_IMPL=\"powerpc/pci_machdep.h\"
. endif
CPPFLAGS+=      -DPPC_INTR_IMPL=${PPC_INTR_IMPL}
CPPFLAGS+=      -DPPC_PCI_MACHDEP_IMPL=${DPPC_PCI_MACHDEP_IMPL}

. ifdef PPC_IBM4XX
CPPFLAGS+=      -DPPC_IBM4XX
. elifdef PPC_BOOKE
CPPFLAGS+=      -DPPC_BOOKE
. else
CPPFLAGS+=      -DPPC_OEA
. endif

.endif


_YKMSRCS=	${SRCS:M*.[ly]:C/\..$/.c/} ${YHEADER:D${SRCS:M*.y:.y=.h}}
DPSRCS+=	${_YKMSRCS}
CLEANFILES+=	${_YKMSRCS}

.if exists($S/../sys/modules/xldscripts/kmodule)
KMODSCRIPT=	$S/../sys/modules/xldscripts/kmodule
.else
KMODSCRIPT=	${DESTDIR}/usr/libdata/ldscripts/kmodule
.endif

PROG?=		${KMOD}.kmod

##### Build rules
realall:	${PROG}

.if (defined(USE_COMBINE) && ${USE_COMBINE} != "no" && !commands(${_P}) \
   && !defined(NOCOMBINE.${_P}) && !defined(NOCOMBINE))
.for f in ${SRCS:N*.h:N*.sh:N*.fth:C/\.[yl]$/.c/g}
.if (${CPPFLAGS.$f:D1} == "1" || ${CPUFLAGS.$f:D2} == "2" \
     || ${COPTS.$f:D3} == "3" || ${OBJCOPTS.$f:D4} == "4" \
     || ${CXXFLAGS.$f:D5} == "5") \
    || ("${f:M*.[cyl]}" == "" || commands(${f:R:S/$/.o/}))
XOBJS+=		${f:R:S/$/.o/}
.else
XSRCS+=		${f}
NODPSRCS+=	${f}
.endif
.endfor

.if !empty(XOBJS)
${XOBJS}:	${DPSRCS}
.endif

${PROG}: ${XOBJS} ${XSRCS} ${DPSRCS} ${DPADD}
	${_MKTARGET_LINK}
	${CC} ${LDFLAGS} -nostdlib -MD -combine -r -Wl,-T,${KMODSCRIPT},-d \
		-o ${.TARGET} ${CFLAGS} ${CPPFLAGS} ${XOBJS} \
		${XSRCS:@.SRC.@${.ALLSRC:M*.c:M*${.SRC.}}@:O:u} && \
	echo '.-include "${KMOD}.d"' > .depend

.else
OBJS+=		${SRCS:N*.h:N*.sh:R:S/$/.o/g}

${OBJS} ${LOBJS}: ${DPSRCS}

${PROG}: ${OBJS} ${DPADD}
	${_MKTARGET_LINK}
	${CC} ${LDFLAGS} -nostdlib -r -Wl,-T,${KMODSCRIPT},-d \
		-o ${.TARGET} ${OBJS}
.endif

##### Install rules
.if !target(kmodinstall)
.if !defined(KMODULEDIR)
_OSRELEASE!=	${HOST_SH} $S/conf/osrelease.sh -k
# Ensure these are recorded properly in METALOG on unprived installes:
KMODULEARCHDIR?= ${MACHINE}
_INST_DIRS=	${DESTDIR}/stand/${KMODULEARCHDIR}
_INST_DIRS+=	${DESTDIR}/stand/${KMODULEARCHDIR}/${_OSRELEASE}
_INST_DIRS+=	${DESTDIR}/stand/${KMODULEARCHDIR}/${_OSRELEASE}/modules
KMODULEDIR=	${DESTDIR}/stand/${KMODULEARCHDIR}/${_OSRELEASE}/modules/${KMOD}
.endif
_PROG:=		${KMODULEDIR}/${PROG} # installed path

.if ${MKUPDATE} == "no"
${_PROG}! ${PROG}					# install rule
.if !defined(BUILD) && !make(all) && !make(${PROG})
${_PROG}!	.MADE					# no build at install
.endif
.else
${_PROG}: ${PROG}					# install rule
.if !defined(BUILD) && !make(all) && !make(${PROG})
${_PROG}:	.MADE					# no build at install
.endif
.endif
	${_MKTARGET_INSTALL}
	dirs=${_INST_DIRS:Q}; \
	for d in $$dirs; do \
		${INSTALL_DIR} $$d; \
	done
	${INSTALL_DIR} ${KMODULEDIR}
	${INSTALL_FILE} -o ${KMODULEOWN} -g ${KMODULEGRP} -m ${KMODULEMODE} \
		${.ALLSRC} ${.TARGET}

kmodinstall::	${_PROG}
.PHONY:		kmodinstall
.PRECIOUS:	${_PROG}				# keep if install fails

.undef _PROG
.endif # !target(kmodinstall)

##### Clean rules
CLEANFILES+= a.out [Ee]rrs mklog core *.core ${PROG} ${OBJS} ${LOBJS}

##### Custom rules
lint: ${LOBJS}
.if defined(LOBJS) && !empty(LOBJS)
	${LINT} ${LINTFLAGS} ${LDFLAGS:C/-L[  ]*/-L/Wg:M-L*} ${LOBJS} ${LDADD}
.endif

##### Pull in related .mk logic
LINKSOWN?= ${KMODULEOWN}
LINKSGRP?= ${KMODULEGRP}
LINKSMODE?= ${KMODULEMODE}
.include <bsd.man.mk>
.include <bsd.links.mk>
.include <bsd.dep.mk>
.include <bsd.clean.mk>

.-include "$S/arch/${MACHINE_CPU}/include/Makefile.inc"
.-include "$S/arch/${MACHINE}/include/Makefile.inc"
