#include <assert.h>
#include <fenv.h>

#include "fpu_cw.h"

int fegetround(void)
{
	u16_t cw;

	/* read and categorize FPUCW */
	cw = fpu_cw_get();
	switch (cw & FPUCW_ROUNDING_CONTROL)
	{
		case FPUCW_ROUNDING_CONTROL_NEAREST: return FE_TONEAREST;
		case FPUCW_ROUNDING_CONTROL_DOWN:    return FE_DOWNWARD;
		case FPUCW_ROUNDING_CONTROL_UP:      return FE_UPWARD;
		case FPUCW_ROUNDING_CONTROL_TRUNC:   return FE_TOWARDZERO;
	}

	/* each case has been handled, otherwise the constants are wrong */
	assert(0);
	return -1;
}
