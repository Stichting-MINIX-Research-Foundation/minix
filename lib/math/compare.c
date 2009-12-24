#include <assert.h>
#include <math.h>

/* functions missing here are architecture-specific and are in i386/float */

int isfinite(double x)
{
	/* return value based on classification */
	switch (fpclassify(x))
	{
		case FP_INFINITE:
		case FP_NAN:
			return 0;

		case FP_NORMAL:
		case FP_SUBNORMAL:
		case FP_ZERO:
			return 1;
	}

	/* if we get here, fpclassify is buggy */
	assert(0);
	return -1;
}

int isinf(double x)
{
	return fpclassify(x) == FP_INFINITE;
}

int isnan(double x)
{
	return fpclassify(x) == FP_NAN;
}

int isnormal(double x)
{
	return fpclassify(x) == FP_NORMAL;
}
