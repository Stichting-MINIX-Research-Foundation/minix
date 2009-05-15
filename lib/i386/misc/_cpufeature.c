
#include <stdint.h>
#include <minix/minlib.h>
#include <minix/cpufeature.h>
#include <sys/vm_i386.h>

int _cpufeature(int cpufeature)
{
	u32_t cpuid_feature_edx = 0;
	int proc;

	proc = getprocessor();

	/* If processor supports CPUID and its CPUID supports enough
	 * parameters, retrieve EDX feature flags to test against.
	 */
	if(proc >= 586) {
		u32_t params, a, b, c, d;
		_cpuid(0, &params, &b, &c, &d);
		if(params > 0) {
			_cpuid(1, &a, &b, &c, &cpuid_feature_edx);
		}
	}

	switch(cpufeature) {
		case _CPUF_I386_PSE:
			return cpuid_feature_edx & CPUID1_EDX_PSE;
		case _CPUF_I386_PGE:
			return cpuid_feature_edx & CPUID1_EDX_PGE;
	}

	return 0;
}

