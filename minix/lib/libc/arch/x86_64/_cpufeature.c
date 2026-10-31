#include <sys/types.h>
#include <stdint.h>
#include <minix/minlib.h>
#include <minix/cpufeature.h>
#include <machine/vm.h>
#include <string.h>

int _cpufeature(int cpufeature)
{
	u32_t eax, ebx, ecx, edx;
	u32_t ef_eax = 0, ef_ebx = 0, ef_ecx = 0, ef_edx = 0;
	unsigned int family, model, stepping;
	int is_intel = 0, is_amd = 0;

	eax = ebx = ecx = edx = 0;

	/* We assume >= pentium for cpuid */
	eax = 0;
	_cpuid(&eax, &ebx, &ecx, &edx);
	if(eax > 0) {
		char vendor[12];
		memcpy(vendor,   &ebx, sizeof(ebx));
		memcpy(vendor+4, &edx, sizeof(edx));
		memcpy(vendor+8, &ecx, sizeof(ecx));
		if(!strncmp(vendor, "GenuineIntel", sizeof(vendor)))
			is_intel = 1;
		if(!strncmp(vendor, "AuthenticAMD", sizeof(vendor)))
			is_amd = 1;
		eax = 1;
		_cpuid(&eax, &ebx, &ecx, &edx);
	} else return 0;

	stepping   =  eax        & 0xf;
	model    = (eax >>  4) & 0xf;

	if(model == 0xf || model == 0x6) {
		model += ((eax >> 16) & 0xf) << 4;
	}

	family   = (eax >>  8) & 0xf;

	if(family == 0xf) {
		family += (eax >> 20) & 0xff;
	}

	if(is_amd) {
		ef_eax = 0x80000001;
		_cpuid(&ef_eax, &ef_ebx, &ef_ecx, &ef_edx);
	}

	switch(cpufeature) {
		case _CPUF_I386_PSE:
			return edx & CPUID1_EDX_PSE;
		case _CPUF_I386_PAE:
			return edx & CPUID1_EDX_PAE;
		case _CPUF_I386_PGE:
			return edx & CPUID1_EDX_PGE;
		case _CPUF_I386_APIC_ON_CHIP:
			return edx & CPUID1_EDX_APIC_ON_CHIP;
		case _CPUF_I386_TSC:
			return edx & CPUID1_EDX_TSC;
		case _CPUF_I386_FPU:
			return edx & CPUID1_EDX_FPU;
#define SSE_FULL_EDX (CPUID1_EDX_FXSR | CPUID1_EDX_SSE | CPUID1_EDX_SSE2)
#define SSE_FULL_ECX (CPUID1_ECX_SSE3 | CPUID1_ECX_SSSE3 | \
	CPUID1_ECX_SSE4_1 | CPUID1_ECX_SSE4_2)
		case _CPUF_I386_SSE1234_12:
			return	(edx & SSE_FULL_EDX) == SSE_FULL_EDX &&
				(ecx & SSE_FULL_ECX) == SSE_FULL_ECX;
		case _CPUF_I386_FXSR:
			return edx & CPUID1_EDX_FXSR;
		case _CPUF_I386_SSE:
			return edx & CPUID1_EDX_SSE;
		case _CPUF_I386_SSE2:
			return edx & CPUID1_EDX_SSE2;
		case _CPUF_I386_SSE3:
			return ecx & CPUID1_ECX_SSE3;
		case _CPUF_I386_SSSE3:
			return ecx & CPUID1_ECX_SSSE3;
		case _CPUF_I386_SSE4_1:
			return ecx & CPUID1_ECX_SSE4_1;
		case _CPUF_I386_SSE4_2:
			return ecx & CPUID1_ECX_SSE4_2;
		case _CPUF_I386_HTT:
			return edx & CPUID1_EDX_HTT;
		case _CPUF_I386_HTT_MAX_NUM:
			return (ebx >> 16) & 0xff;
		case _CPUF_I386_SYSENTER:
			if(!is_intel) return 0;
			if(!(edx & CPUID1_EDX_SYSENTER)) return 0;
			if(family == 6 && model < 3 && stepping < 3) return 0;
			return 1;
		case _CPUF_I386_SYSCALL:
			if(!is_amd) return 0;
			if(!(ef_edx & CPUID_EF_EDX_SYSENTER)) return 0;
			return 1;
	}

	return 0;
}

