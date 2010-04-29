
#include <stdint.h>
#include <minix/minlib.h>
#include <minix/cpufeature.h>
#include <machine/vm.h>

int _cpufeature(int cpufeature)
{
	u32_t eax, ebx, ecx, edx;
	int proc;

	eax = ebx = ecx = edx = 0;
	proc = getprocessor();

	/* If processor supports CPUID and its CPUID supports enough
	 * parameters, retrieve EDX feature flags to test against.
	 */
	if(proc >= 586) {
		eax = 0;
		_cpuid(&eax, &ebx, &ecx, &edx);
		if(eax > 0) {
			eax = 1;
			_cpuid(&eax, &ebx, &ecx, &edx);
		}
	}

	switch(cpufeature) {
		case _CPUF_I386_PSE:
			return edx & CPUID1_EDX_PSE;
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
	}

	return 0;
}

