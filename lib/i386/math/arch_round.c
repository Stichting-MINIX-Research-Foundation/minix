#include <errno.h>
#include <fenv.h>
#include <math.h>

#include "fpu_cw.h"
#include "fpu_round.h"

static double rndint(double x, u16_t cw_bits, u16_t cw_mask)
{
	u16_t cw;

	/* set FPUCW to the right value */
	cw = fpu_cw_get();
	fpu_cw_set((cw & cw_mask) | cw_bits);

	/* perform the round */
	fpu_rndint(&x);
	
	/* restore FPUCW */
	fpu_cw_set(cw);
	return x;
}

double nearbyint(double x)
{
	/* round, disabling floating point precision error */
	return rndint(x, FPUCW_EXCEPTION_MASK_PM, ~0);
}

double remainder(double x, double y)
{
	int xclass, yclass;

	/* check arguments */
	xclass = fpclassify(x);
	yclass = fpclassify(y);
	if (xclass == FP_NAN || yclass == FP_NAN)
		return NAN;

	if (xclass == FP_INFINITE || yclass == FP_ZERO)
	{
		errno = EDOM;
		return NAN;
	}

	/* call the assembly implementation */
	fpu_remainder(&x, y);
	return x;
}

double trunc(double x)
{
	/* round in truncate mode, disabling floating point precision error */
	return rndint(x, 
		FPUCW_EXCEPTION_MASK_PM | FPUCW_ROUNDING_CONTROL_TRUNC, 
		~FPUCW_ROUNDING_CONTROL);
}
