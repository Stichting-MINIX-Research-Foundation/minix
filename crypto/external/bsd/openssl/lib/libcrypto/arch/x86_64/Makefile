#	$NetBSD: Makefile,v 1.8 2015/05/16 22:23:31 joerg Exp $

.include "bsd.own.mk"

CRYPTODIST=${NETBSDSRCDIR}/crypto
.include "${NETBSDSRCDIR}/crypto/Makefile.openssl"

.if make(regen) && ${HAVE_LLVM:U} == "yes"
CC+= -fno-integrated-as
.endif

regen:
	for i in $$(find ${OPENSSLSRC} -name \*${MACHINE_ARCH}.pl) \
		$$(find ${OPENSSLSRC}/crypto/bn/asm -name ${MACHINE_ARCH}-\*.pl) \
		${OPENSSLSRC}/crypto/${MACHINE_ARCH}cpuid.pl ; do \
                (echo "#include <machine/asm.h>"; CC=${CC:Q} perl $$i elf | sed \
		    -e 's/\(OPENSSL[A-Za-z0-9_+]*\)(%rip)/\1@GOTPCREL(%rip)/' \
		    -e 's/.hidden	OPENSSL_cpuid_setup/.globl	OPENSSL_cpuid_setup/' \
		    -e 's/call	OPENSSL_cpuid_setup/call	PIC_PLT(OPENSSL_cpuid_setup)/') \
		> $$(basename $$i .pl).S; \
	done
