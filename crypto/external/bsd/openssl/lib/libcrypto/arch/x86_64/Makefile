#	$NetBSD: Makefile,v 1.6 2012/08/04 11:03:34 christos Exp $

.include "bsd.own.mk"

CRYPTODIST=${NETBSDSRCDIR}/crypto
.include "${NETBSDSRCDIR}/crypto/Makefile.openssl"

regen:
	for i in $$(find ${OPENSSLSRC} -name \*${MACHINE_ARCH}.pl) \
		${OPENSSLSRC}/crypto/${MACHINE_ARCH}cpuid.pl ; do \
                (echo "#include <machine/asm.h>"; perl $$i elf | sed \
		    -e 's/\(OPENSSL[A-Za-z0-9_+]*\)(%rip)/\1@GOTPCREL(%rip)/' \
		    -e 's/.hidden	OPENSSL_cpuid_setup/.globl	OPENSSL_cpuid_setup/' \
		    -e 's/call	OPENSSL_cpuid_setup/call	PIC_PLT(OPENSSL_cpuid_setup)/') \
		> $$(basename $$i .pl).S; \
	done
