#	$NetBSD: Makefile,v 1.43 2012/10/23 16:08:12 dyoung Exp $

# Makefile for i386 tags file and boot blocks

TI386=	${SYSDIR}/arch/i386/tags
SI386=	${SYSDIR}/arch/i386/acpi/*.[ch] \
	${SYSDIR}/arch/i386/eisa/*.[ch] \
	${SYSDIR}/arch/i386/i386/*.[ch] \
	${SYSDIR}/arch/i386/include/*.h \
	${SYSDIR}/arch/i386/isa/*.[ch] \
	${SYSDIR}/arch/i386/mca/*.[ch] \
	${SYSDIR}/arch/i386/pci/*.[ch] \
	${SYSDIR}/arch/i386/pnpbios/*.[ch] \
	${SYSDIR}/external/isc/atheros_hal/dist/*.[ch] \
	${SYSDIR}/external/isc/atheros_hal/dist/*/*.[ch] \
	${SYSDIR}/external/isc/atheros_hal/ic/*.[ch]
SI386+=	${SYSDIR}/arch/x86/x86/*.[ch] \
	${SYSDIR}/arch/x86/acpi/*.[ch] \
	${SYSDIR}/arch/x86/include/*.h \
	${SYSDIR}/arch/x86/isa/*.[ch] \
	${SYSDIR}/arch/x86/pci/*.[ch] \
	${SYSDIR}/arch/xen/include/*.h \
	${SYSDIR}/arch/xen/i386/*.[ch] \
	${SYSDIR}/arch/xen/x86/*.[ch] \
	${SYSDIR}/arch/xen/xen/*.[ch] \
	${SYSDIR}/arch/xen/xenbus/*.[ch]
AI386=	${SYSDIR}/arch/i386/i386/*.S \
	${SYSDIR}/../common/lib/libc/arch/i386/*/*.S

# Directories in which to place tags links
DI386=	i386 eisa isa include pci

.if !defined(__MINIX)
.include "../../kern/Make.tags.inc"

tags:
	-rm -f ${TI386}
	-echo ${SI386} | xargs $(TOOL_CTAGS) -wadtf ${TI386}
	-find -H ${SYSDIR}/external/bsd/acpica/dist/ -name '*.[ch]' | \
	    sort -t / -u | xargs $(TOOL_CTAGS) -wadtf ${TI386}
	-${FINDCOMM} | xargs $(TOOL_CTAGS) -wadtf ${TI386}
	egrep "^ENTRY(.*)|^ALTENTRY(.*)" ${AI386} | \
	    ${TOOL_SED} -e \
	"s;\([^:]*\):\([^(]*\)(\([^, )]*\)\(.*\);\3	\1	/^\2(\3\4$$/;" \
		>> ${TI386}
	sort -o ${TI386} ${TI386}

links:
	-for i in ${DI386}; do \
	    (cd $$i && rm -f tags; ln -s ../tags tags); done
.endif # !defined(__MINIX)


SUBDIR=	include stand ../x86/include

.include <bsd.subdir.mk>
