#	$NetBSD: Makefile,v 1.12 2017/01/08 17:12:21 christos Exp $

.include "bsd.own.mk"

CRYPTODIST=${NETBSDSRCDIR}/crypto
.include "${NETBSDSRCDIR}/crypto/Makefile.openssl"

.if make(regen) && ${HAVE_LLVM:U} == "yes"
CC+= -fno-integrated-as
.endif

regen:
	for i in $$(find ${OPENSSLSRC} -name \*${MACHINE_ARCH}.pl) \
		$$(find ${OPENSSLSRC}/crypto/bn/asm -name ${MACHINE_ARCH}-\*.pl) \
		${OPENSSLSRC}/crypto/bn/asm/rsaz-avx2.pl \
		${OPENSSLSRC}/crypto/${MACHINE_ARCH}cpuid.pl ; do \
                (set +x; echo "#include <machine/asm.h>"; CC=${CC:Q} perl $$i elf | sed \
		    -e 's/.hidden	OPENSSL_cpuid_setup/.globl	OPENSSL_cpuid_setup/' \
		    -e 's/call	OPENSSL_cpuid_setup/call	PIC_PLT(OPENSSL_cpuid_setup)/' \
		    -e 's/rdseedq/rdseed/') \
		> $$(basename $$i .pl).S; \
	done
