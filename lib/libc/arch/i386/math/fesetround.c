#include <errno.h>
#include <fenv.h>

#include "fpu_cw.h"

int fesetround(int round)
{
	u16_t cw;

	/* read and update FPUCW */
	cw = fpu_cw_get() & ~FPUCW_ROUNDING_CONTROL;
	switch (round)
	{
		case FE_TONEAREST: cw |= FPUCW_ROUNDING_CONTROL_NEAREST; break;
		case FE_DOWNWARD: cw |= FPUCW_ROUNDING_CONTROL_DOWN; break;
		case FE_UPWARD: cw |= FPUCW_ROUNDING_CONTROL_UP; break;
		case FE_TOWARDZERO: cw |= FPUCW_ROUNDING_CONTROL_TRUNC; break;

		default:
			errno = EINVAL;
			return -1;
	}

	/* set FPUCW to the updated value */
	fpu_cw_set(cw);
	return 0;
}
