#include <fenv.h>

#include "fpu_cw.h"
#include "fpu_sw.h"

int feholdexcept(fenv_t *envp)
{
	/* read FPUCW and FPUSW */
	envp->cw = fpu_cw_get();
	envp->sw = fpu_sw_get();
	
	/* update FPUCW to block exceptions */
	fpu_cw_set(envp->cw | FPUCW_EXCEPTION_MASK);

	return 0;
}
