
#include <stdint.h>
#include <minix/minlib.h>
#include <minix/cpufeature.h>
#include <sys/vm_i386.h>

int _cpufeature(int cpufeature)
{
	u32_t cpuid_feature_edx = 0;
	u32_t cpuid_feature_ecx = 0;
	int proc;

	proc = getprocessor();

	/* If processor supports CPUID and its CPUID supports enough
	 * parameters, retrieve EDX feature flags to test against.
	 */
	if(proc >= 586) {
		u32_t params, a, b, c, d;
		_cpuid(0, &params, &b, &c, &d);
		if(params > 0) {
			_cpuid(1, &a, &b, &cpuid_feature_ecx,
					  &cpuid_feature_edx);
		}
	}

	switch(cpufeature) {
		case _CPUF_I386_PSE:
			return cpuid_feature_edx & CPUID1_EDX_PSE;
		case _CPUF_I386_PGE:
			return cpuid_feature_edx & CPUID1_EDX_PGE;
		case _CPUF_I386_APIC_ON_CHIP:
			return cpuid_feature_edx & CPUID1_EDX_APIC_ON_CHIP;
		case _CPUF_I386_TSC:
			return cpuid_feature_edx & CPUID1_EDX_TSC;
		case _CPUF_I386_FPU:
			return cpuid_feature_edx & CPUID1_EDX_FPU;
		case _CPUF_I386_SSEx:
			return (cpuid_feature_edx & (CPUID1_EDX_FXSR |
						     CPUID1_EDX_SSE |
						     CPUID1_EDX_SSE2)) &&
			       (cpuid_feature_ecx & (CPUID1_ECX_SSE3 |
						     CPUID1_ECX_SSSE3 |
						     CPUID1_ECX_SSE4_1 |
						     CPUID1_ECX_SSE4_2));
		case _CPUF_I386_FXSR:
			return cpuid_feature_edx & CPUID1_EDX_FXSR;
		case _CPUF_I386_SSE:
			return cpuid_feature_edx & CPUID1_EDX_SSE;
		case _CPUF_I386_SSE2:
			return cpuid_feature_edx & CPUID1_EDX_SSE2;
		case _CPUF_I386_SSE3:
			return cpuid_feature_ecx & CPUID1_ECX_SSE3;
		case _CPUF_I386_SSSE3:
			return cpuid_feature_ecx & CPUID1_ECX_SSSE3;
		case _CPUF_I386_SSE4_1:
			return cpuid_feature_ecx & CPUID1_ECX_SSE4_1;
		case _CPUF_I386_SSE4_2:
			return cpuid_feature_ecx & CPUID1_ECX_SSE4_2;
	}

	return 0;
}

