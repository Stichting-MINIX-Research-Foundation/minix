#	$NetBSD: Makefile,v 1.8 2017/12/22 07:19:02 maxv Exp $

# Makefile for amd64 tags file and boot blocks

TAMD64=		${SYSDIR}/arch/amd64/tags
SAMD64=		${SYSDIR}/arch/amd64/amd64/*.[ch] \
		${SYSDIR}/arch/amd64/include/*.h \
		${SYSDIR}/external/isc/atheros_hal/dist/*.[ch] \
		${SYSDIR}/external/isc/atheros_hal/dist/*/*.[ch] \
		${SYSDIR}/external/isc/atheros_hal/ic/*.[ch]
SAMD64+=	${SYSDIR}/arch/x86/x86/*.[ch] \
		${SYSDIR}/arch/x86/acpi/*.[ch] \
		${SYSDIR}/arch/x86/include/*.h \
		${SYSDIR}/arch/x86/isa/*.[ch] \
		${SYSDIR}/arch/x86/pci/*.[ch]
AAMD64=		${SYSDIR}/arch/amd64/amd64/*.S \
		${SYSDIR}/arch/amd64/acpi/*.S
# Directories in which to place tags links
DAMD64=		amd64 isa include pci

.if !defined(__MINIX)
.include "../../kern/Make.tags.inc"

tags:
	-rm -f ${TAMD64}
	-echo ${SAMD64} | xargs ctags -wadtf ${TAMD64}
	-find -H ${SYSDIR}/external/intel-public/acpica/dist/ -name '*.[ch]' | \
	    sort -t / -u | xargs ctags -wadtf ${TAMD64}
	-${FINDCOMM} | xargs ctags -wadtf ${TAMD64}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AAMD64} | \
	    ${TOOL_SED} -e \
		"s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TAMD64}
	sort -o ${TAMD64} ${TAMD64}

links:
	-for i in ${DAMD64}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done
.endif # !defined(__MINIX)


SUBDIR=	include stand ../x86/include

.include <bsd.subdir.mk>
